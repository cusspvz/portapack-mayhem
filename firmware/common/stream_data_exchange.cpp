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

#include "stream_data_exchange.hpp"
#include "portapack_shared_memory.hpp"

StreamDataExchange::StreamDataExchange(const stream_exchange_direction direction) : _direction{direction}
{
#if defined(LPC43XX_M0)
    last_instance = this;
#endif

    buffer_from_baseband_to_application = nullptr;
    buffer_from_application_to_baseband = nullptr;

    if (direction == STREAM_EXCHANGE_DUPLEX)
    {
        // use the shared data to setup the circular buffers for duplex comms
        buffer_from_baseband_to_application = new CircularBuffer(&(shared_memory.bb_data.data[0]), 256);
        buffer_from_application_to_baseband = new CircularBuffer(&(shared_memory.bb_data.data[256]), 256);
    }

    if (direction == STREAM_EXCHANGE_APP_TO_BB)
        buffer_from_application_to_baseband = new CircularBuffer(&(shared_memory.bb_data.data[0]), 512);

    if (direction == STREAM_EXCHANGE_BB_TO_APP)
        buffer_from_baseband_to_application = new CircularBuffer(&(shared_memory.bb_data.data[0]), 512);

    if (buffer_from_baseband_to_application != nullptr)
        buffer_from_baseband_to_application->clear_data();

    if (buffer_from_application_to_baseband != nullptr)
        buffer_from_application_to_baseband->clear_data();
}

StreamDataExchange::StreamDataExchange(const StreamDataExchangeConfig *config) : _direction{config->direction},
                                                                                 buffer_from_baseband_to_application{config->buffer_from_baseband_to_application},
                                                                                 buffer_from_application_to_baseband{config->buffer_from_application_to_baseband}
{
#if defined(LPC43XX_M0)
    last_instance = this;
#endif
};

StreamDataExchange::~StreamDataExchange()
{
    // delete buffer_from_baseband_to_application;
    // delete buffer_from_application_to_baseband;

#if defined(LPC43XX_M0)
    last_instance = nullptr;

    if (isr_thread)
    {
        chThdTerminate(isr_thread);
        isr_thread = nullptr;
    }
#endif
};

// Methods for the Application
#if defined(LPC43XX_M0)
Result<size_t, Error> StreamDataExchange::read(void *p, const size_t count)
{
    // cannot read if we're only writing to the baseband
    if (_direction == STREAM_EXCHANGE_APP_TO_BB)
        return {0};

    // suspend in case the target buffer is full
    while (buffer_from_application_to_baseband->is_empty())
    {
        wait_for_isr_event();

        if (chThdShouldTerminate())
            return {0};
    }

    auto result = buffer_from_baseband_to_application->read(p, count);
    bytes_read += result;
    return {result};
}

Result<size_t, Error> StreamDataExchange::write(const void *p, const size_t count)
{
    // cannot write if we're only reading from the baseband
    if (_direction == STREAM_EXCHANGE_BB_TO_APP)
        return {0};

    // suspend in case the target buffer is full
    while (buffer_from_application_to_baseband->is_full())
    {
        wait_for_isr_event();

        if (chThdShouldTerminate())
            return {0};
    }

    auto result = buffer_from_application_to_baseband->write(p, count);
    bytes_written += result;
    return {result};
}

void StreamDataExchange::setup_baseband_stream()
{
    if (baseband_ready)
        return;

    baseband_ready = true;

    // push an event to the baseband to setup the stream
    baseband::set_stream_data_exchange(new StreamDataExchangeConfig{
        .direction = _direction,
        .buffer_from_baseband_to_application = buffer_from_baseband_to_application,
        .buffer_from_application_to_baseband = buffer_from_application_to_baseband});
}

void StreamDataExchange::wait_for_isr_event()
{
    // Put thread to sleep, woken up by M4 IRQ
    chSysLock();
    isr_thread = chThdSelf();
    chSchGoSleepTimeoutS(THD_STATE_SUSPENDED, 500);
    chSysUnlock();
}

void StreamDataExchange::wakeup_isr()
{
    auto thread_tmp = isr_thread;
    if (thread_tmp)
    {
        isr_thread = nullptr;
        chSchReadyI(thread_tmp);
    }
}
#endif

// Methods for the Baseband
#if defined(LPC43XX_M4)
Result<size_t, Error> StreamDataExchange::read(void *p, const size_t count)
{
    // cannot read if we're only writing to the baseband
    if (_direction == STREAM_EXCHANGE_BB_TO_APP)
        return {0};

    // signal the application from the baseband that we need more data
    // if (!buffer_from_application_to_baseband->is_full())
    //     creg::m4txevent::assert_event();

    auto result = buffer_from_application_to_baseband->read(p, count);
    bytes_read += result;
    return {result};
}

Result<size_t, Error> StreamDataExchange::write(const void *p, const size_t count)
{
    // cannot write if we're only reading from the baseband
    if (_direction == STREAM_EXCHANGE_APP_TO_BB)
        return {0};

    // signal the application from the baseband that we need it to read the data
    // if (!buffer_from_baseband_to_application->is_empty())
    //     creg::m4txevent::assert_event();

    auto result = buffer_from_baseband_to_application->write(p, count);
    bytes_written += result;
    return {result};
}
#endif

#if defined(LPC43XX_M0)

StreamReader::StreamReader(std::unique_ptr<stream::Reader> reader) : reader{std::move(reader)}
{
    thread = chThdCreateFromHeap(NULL, 512, NORMALPRIO + 10, StreamReader::static_fn, this);
};

StreamReader::~StreamReader()
{
    if (thread)
    {
        if (thread->p_state != THD_STATE_FINAL)
        {
            chThdTerminate(thread);
            chThdWait(thread);
        }

        thread = nullptr;
    }
};

uint32_t StreamReader::run()
{
    uint8_t *buffer_block = new uint8_t[128];

    while (!chThdShouldTerminate())
    {
        size_t block_bytes = 0;
        size_t block_bytes_written = 0;

        if (!reader)
            return NO_READER;

        // read from reader
        auto read_result = reader->read(buffer_block, sizeof(*buffer_block));

        if (read_result.is_error())
            return READ_ERROR;

        if (read_result.value() == 0) // end of stream
            return END_OF_STREAM;

        block_bytes = read_result.value();

        while (block_bytes_written < block_bytes && !chThdShouldTerminate())
        {
            // write to baseband
            auto write_result = data_exchange.write(buffer_block + block_bytes_written, block_bytes - block_bytes_written);

            if (read_result.is_error())
                return WRITE_ERROR;

            if (write_result.value() > 0)
                block_bytes_written += write_result.value();

            if (data_exchange.buffer_from_application_to_baseband->is_full())
                data_exchange.setup_baseband_stream();
        }

        data_exchange.setup_baseband_stream();
    }

    return TERMINATED;
};

msg_t StreamReader::static_fn(void *arg)
{
    auto obj = static_cast<StreamReader *>(arg);

    const uint32_t return_code = obj->run();

    // TODO: adapt this to the new stream reader interface
    StreamReaderThreadDoneMessage message{return_code};
    EventDispatcher::send_message(message);

    return 0;
};

#endif
