/*
 * Copyright (C) 2016 Jared Boone, ShareBrained Technology, Inc.
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

#include "io.hpp"

#include <cstdint>

enum OOKReaderReadType
{
	OOK_READER_COMPLETED = 0,
	OOK_READER_READING_FRAGMENT = 1,
	OOK_READER_READING_PAUSES = 2,
};

using OOKReaderError = Error;

class OOKReader : public stream::Reader
{
public:
	OOKReader() = default;

	OOKReader(const OOKReader &) = delete;
	OOKReader &operator=(const OOKReader &) = delete;
	OOKReader &operator=(OOKReader &&) = delete;

	Result<uint64_t, OOKReaderError> read(void *const buffer, const uint64_t bytes) override;
	uint64_t length();

protected:
	uint64_t bytes_read{0};

	std::string &fragments;
	uint8_t repeat_total{1};
	uint16_t pause_total{0};

	// ongoing read vars
	OOKReaderReadType read_type = OOK_READER_READING_FRAGMENT;
	uint8_t fragment_bit_count{0};
	uint8_t repeat_bit_count{0};
	uint8_t pause_bit_count{0};
};
