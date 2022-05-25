/*
 * Copyright (C) 2022 José Moreira @cusspvz
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <cstring>
#include "portapack_shared_memory.hpp"

#if defined(LPC43XX_M0)
#include "io.hpp"
#include "baseband_api.hpp"
#endif

#include "lpc43xx_cpp.hpp"
using namespace lpc43xx;

class StreamDataExchange
{
public:
    StreamDataExchange(const stream_exchange_direction direction) : _direction{direction}
    {
#if defined(LPC43XX_M0)
        last_instance = this;
#endif

        _buffer_from_baseband_to_application = nullptr;
        _buffer_from_application_to_baseband = nullptr;

        if (direction == STREAM_EXCHANGE_DUPLEX)
        {
            // use the shared data to setup the circular buffers for duplex comms
            _buffer_from_baseband_to_application = new CircularBuffer(&(shared_memory.bb_data.data[0]), 256);
            _buffer_from_application_to_baseband = new CircularBuffer(&(shared_memory.bb_data.data[256]), 256);
        }
        if (direction == STREAM_EXCHANGE_APP_TO_BB)
        {
            _buffer_from_application_to_baseband = new CircularBuffer(&(shared_memory.bb_data.data[0]), 512);
        }
        else if (direction == STREAM_EXCHANGE_BB_TO_APP)
        {
            _buffer_from_baseband_to_application = new CircularBuffer(&(shared_memory.bb_data.data[0]), 512);
        }
    }

    StreamDataExchange(StreamDataExchangeConfig *const config) : _direction{config->direction},
                                                                 _buffer_from_baseband_to_application{config->buffer_from_baseband_to_application},
                                                                 _buffer_from_application_to_baseband{config->buffer_from_application_to_baseband}
    {
#if defined(LPC43XX_M0)
        last_instance = this;
#endif
    }

    ~StreamDataExchange()
    {
#if defined(LPC43XX_M0)
        last_instance = nullptr;

        if (thread)
        {
            chThdTerminate(thread);
            thread = nullptr;
        }
#endif
    };

    StreamDataExchange(const StreamDataExchange &) = delete;
    StreamDataExchange(StreamDataExchange &&) = delete;
    StreamDataExchange &operator=(const StreamDataExchange &) = delete;
    StreamDataExchange &operator=(StreamDataExchange &&) = delete;

    // size_t read(void *p, const size_t count);
    // size_t write(const void *p, const size_t count);

// Methods for the Application
#if defined(LPC43XX_M0)
    size_t read(void *p, const size_t count)
    {
        // cannot read if we're only writing to the baseband
        if (_direction == STREAM_EXCHANGE_APP_TO_BB)
            return 0;

        // suspend in case the target buffer is full
        while (_buffer_from_application_to_baseband->is_empty())
        {
            wait_for_isr_event();

            if (chThdShouldTerminate())
                return 0;
        }

        return _buffer_from_baseband_to_application->read(p, count);
    }

    size_t write(const void *p, const size_t count)
    {
        // cannot write if we're only reading from the baseband
        if (_direction == STREAM_EXCHANGE_BB_TO_APP)
            return 0;

        // suspend in case the target buffer is full
        while (_buffer_from_application_to_baseband->is_full())
        {
            wait_for_isr_event();

            if (chThdShouldTerminate())
                return 0;
        }

        return _buffer_from_application_to_baseband->write(p, count);
    }

    void setup_baseband_stream()
    {
        // push an event to the baseband to setup the stream
        baseband::set_stream_data_exchange(new StreamDataExchangeConfig{
            .direction = _direction,
            .buffer_from_baseband_to_application = _buffer_from_baseband_to_application,
            .buffer_from_application_to_baseband = _buffer_from_application_to_baseband});
    }

    void teardown_baseband_stream()
    {
        // push an event to the baseband to teardown the stream
        baseband::set_stream_data_exchange(nullptr);
    }

    Thread *thread{nullptr};

    void wait_for_isr_event()
    {
        // Put thread to sleep, woken up by M4 IRQ
        chSysLock();
        thread = chThdSelf();
        chSchGoSleepTimeoutS(THD_STATE_SUSPENDED, 250);
        chSysUnlock();
    }

    void wakeup_isr()
    {
        auto thread_tmp = thread;
        if (thread_tmp)
        {
            thread = nullptr;
            chSchReadyI(thread_tmp);
        }
    }
#endif

// Methods for the Baseband
#if defined(LPC43XX_M4)
    size_t read(void *p, const size_t count)
    {
        // cannot read if we're only writing to the baseband
        if (_direction == STREAM_EXCHANGE_BB_TO_APP)
            return 0;

        // signal the application from the baseband that we need more data
        if (!_buffer_from_application_to_baseband->is_full())
            creg::m4txevent::assert_event();

        return _buffer_from_application_to_baseband->read(p, count);
    }

    size_t write(const void *p, const size_t count)
    {
        // cannot write if we're only reading from the baseband
        if (_direction == STREAM_EXCHANGE_APP_TO_BB)
            return 0;

        // signal the application from the baseband that we need it to read the data
        if (!_buffer_from_baseband_to_application->is_empty())
            creg::m4txevent::assert_event();

        return _buffer_from_baseband_to_application->write(p, count);
    }
#endif

#if defined(LPC43XX_M0)
    inline static StreamDataExchange *last_instance;
    static void handle_isr()
    {
        if (last_instance)
            last_instance->wakeup_isr();
    }
#endif

private:
    stream_exchange_direction _direction{};

    // buffer to store data in and out
    CircularBuffer *_buffer_from_baseband_to_application{nullptr};
    CircularBuffer *_buffer_from_application_to_baseband{nullptr};
};

#if defined(LPC43XX_M0)

class StreamReader
{
public:
    StreamReader(std::unique_ptr<stream::Reader> reader) : reader{std::move(reader)}
    {
        thread = chThdCreateFromHeap(NULL, 512, NORMALPRIO + 10, StreamReader::static_fn, this);
    };
    ~StreamReader()
    {
        if (thread)
        {
            chThdTerminate(thread);
            chThdWait(thread);
            thread = nullptr;
        }
    };

    StreamReader(const StreamReader &) = delete;
    StreamReader(StreamReader &&) = delete;
    StreamReader &operator=(const StreamReader &) = delete;
    StreamReader &operator=(StreamReader &&) = delete;

    enum StreamReader_return
    {
        READ_ERROR = 0,
        END_OF_STREAM,
        TERMINATED
    };

    constexpr static size_t buffer_size = 128; // bytes
    uint8_t buffer[buffer_size];

private:
    std::unique_ptr<stream::Reader> reader;
    StreamDataExchange data_exchange{STREAM_EXCHANGE_APP_TO_BB};
    Thread *thread{nullptr};

    uint32_t run()
    {
        while (true)
        {
            if (chThdShouldTerminate())
                break;

            // read from reader
            auto read_result = reader->read(buffer, buffer_size);

            if (read_result.is_error())
                return READ_ERROR;

            if (read_result.value() == 0) // end of stream
                return END_OF_STREAM;

            size_t bytes_written = 0;
            while (bytes_written < read_result.value())
            {
                // write to baseband
                bytes_written += data_exchange.write(&buffer[bytes_written], read_result.value() - bytes_written);
            }
        }
    };

    inline static msg_t static_fn(void *arg)
    {
        auto obj = static_cast<StreamReader *>(arg);
        const uint32_t return_code = obj->run();

        // TODO: adapt this to the new stream reader interface
        StreamReaderThreadDoneMessage message{return_code};
        EventDispatcher::send_message(message);

        return 0;
    };
};

#endif
