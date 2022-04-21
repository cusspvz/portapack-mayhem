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

#include "encoders.hpp"
#include "io.hpp"
#include <functional>
#include <cstdint>

using namespace encoders;

enum OOKEncoderReaderReadType
{
	OOK_READER_COMPLETED = 0,
	OOK_READER_READING_FRAGMENT = 1,
	OOK_READER_READING_PAUSES = 2,
};

class OOKEncoderReader : public stream::Reader
{
public:
	std::function<void(OOKEncoderReader &)> on_before_frame_fragment_usage{};

	OOKEncoderReader() = default;

	OOKEncoderReader(const OOKEncoderReader &) = delete;
	OOKEncoderReader &operator=(const OOKEncoderReader &) = delete;
	OOKEncoderReader &operator=(OOKEncoderReader &&) = delete;

	Result<uint64_t, Error> read(void *const buffer, const uint64_t bytes) override;
	// uint64_t length();

	// TODO: move this to protected
	uint8_t repeat_total{1};
	uint16_t pause_total{0};
	std::string frame_fragments = "0";
	void reset();

protected:
	uint64_t bytes_read{0};

private:
	// repetition and pauses
	uint8_t _repeat_bit_count{0};
	uint8_t _pause_bit_count{0};

	// ongoing read vars
	uint8_t _fragment_bit_count{0};
	OOKEncoderReaderReadType _read_type = OOK_READER_READING_FRAGMENT;
};
