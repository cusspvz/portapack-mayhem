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

#include <string>

#pragma once

struct Error
{
    // constexpr Error() = default;
    constexpr Error(uint32_t code_, const char * message_) : code{code_}, message{message_} {}
    Error(uint32_t code_ = 0, const std::string& message_ = "") : code{code_}
    {
        if (message_.empty())
            message = std::string("Error " + std::to_string(code)).c_str();
        else 
            message = message_.c_str();  
    }

    const uint32_t code{0};
    const char *message{nullptr};
};