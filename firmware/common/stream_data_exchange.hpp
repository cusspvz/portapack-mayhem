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
#include "io.hpp"
#include "message.hpp"

#include "lpc43xx_cpp.hpp"
using namespace lpc43xx;

#if defined(LPC43XX_M0)
#include "baseband_api.hpp"
#endif

class StreamDataExchange : public stream::Reader, public stream::Writer

{
public:
    StreamDataExchange(const stream_exchange_direction direction);
    StreamDataExchange(const StreamDataExchangeConfig *config);

    ~StreamDataExchange();
    StreamDataExchange(const StreamDataExchange &) = delete;
    StreamDataExchange(StreamDataExchange &&) = delete;
    StreamDataExchange &operator=(const StreamDataExchange &) = delete;
    StreamDataExchange &operator=(StreamDataExchange &&) = delete;

    Result<size_t, Error> read(void *p, const size_t count);
    Result<size_t, Error> write(const void *p, const size_t count);

    size_t bytes_read{0};
    size_t bytes_written{0};

#if defined(LPC43XX_M0)
    Thread *isr_thread{nullptr};
    void setup_baseband_stream();
    void wait_for_isr_event();
    void wakeup_isr();

    inline static StreamDataExchange *last_instance;
    static void handle_isr()
    {
        if (last_instance)
            last_instance->wakeup_isr();
    }
#endif

    // buffer to store data in and out
    CircularBuffer *buffer_from_baseband_to_application{nullptr};
    CircularBuffer *buffer_from_application_to_baseband{nullptr};

private:
#if defined(LPC43XX_M0)
    bool baseband_ready = false;
#endif
    stream_exchange_direction _direction;
};

#if defined(LPC43XX_M0)

#include "event_m0.hpp"
class StreamReader
{
public:
    StreamReader(std::unique_ptr<stream::Reader> reader);
    ~StreamReader();

    StreamReader(const StreamReader &) = delete;
    StreamReader(StreamReader &&) = delete;
    StreamReader &operator=(const StreamReader &) = delete;
    StreamReader &operator=(StreamReader &&) = delete;

    enum StreamReader_return
    {
        END_OF_STREAM = 0,
        NO_READER,
        READ_ERROR,
        WRITE_ERROR,
        TERMINATED
    };

private:
    std::unique_ptr<stream::Reader> reader{nullptr};
    StreamDataExchange data_exchange{STREAM_EXCHANGE_APP_TO_BB};
    Thread *thread{nullptr};

    static msg_t static_fn(void *arg);
    uint32_t run();
};

#endif
