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
#pragma once

#include <cstring>
#include "io.hpp"
#include "error.hpp"
#include "event_m0.hpp"
#include "io_exchange.hpp"

#include "lpc43xx_cpp.hpp"
using namespace lpc43xx;

namespace stream
{
    class StreamWriter
    {
    public:
        StreamWriter(IoExchange *io_exchange, std::unique_ptr<Writer> writer);
        ~StreamWriter();

        StreamWriter(const StreamWriter &) = delete;
        StreamWriter(StreamWriter &&) = delete;
        StreamWriter &operator=(const StreamWriter &) = delete;
        StreamWriter &operator=(StreamWriter &&) = delete;

    private:
        IoExchange *io_exchange;
        std::unique_ptr<Writer> writer{nullptr};
        Thread *thread{nullptr};

        static msg_t static_fn(void *arg);
        const Error run();
    };

} /* namespace stream */