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

#include "io_ook.hpp"

// uint64_t OOKEncoderReader::length()
// {
// 	return (frame_fragments.length() + pause_total) * repeat_total;
// };

void OOKEncoderReader::reset()
{
	bytes_read = 0;

	// repetition and pauses
	repeat_total = 1;
	_repeat_bit_count = 0;
	pause_total = 0;
	_pause_bit_count = 0;

	// ongoing read vars
	_fragment_bit_count = 0;
	_read_type = OOK_READER_READING_FRAGMENT;
}

Result<uint64_t, Error> OOKEncoderReader::read(void *const buffer, const uint64_t bytes)
{
	// assuming 8bit buffer array
	// TODO: we might need to measure the bit size of the buffer
	uint64_t bytes_read = 0;
	uint8_t *rbuff = (uint8_t *)buffer;

	// start filling the buffer
	for (size_t byte_i = 0; byte_i < bytes; byte_i++)
	{
		uint8_t byte = 0;

		if (_read_type != OOK_READER_COMPLETED)
		{
			for (uint8_t bit = 8; bit > 0; bit--)
			{
				bool turn_bit_on = false;

				if (_read_type == OOK_READER_READING_FRAGMENT)
				{
					if (_fragment_bit_count == 0 && on_before_frame_fragment_usage)
					{
						// Allow the app to generate the fragment on demand, or at a single shot
						on_before_frame_fragment_usage(*this);
					};

					turn_bit_on = frame_fragments[_fragment_bit_count] == '1';

					_fragment_bit_count++;

					// if completed, jump to either a pause or completed
					if (_fragment_bit_count == frame_fragments.length())
					{
						if (_repeat_bit_count < repeat_total)
						{
							// start pause
							_pause_bit_count = 0;
							_read_type = OOK_READER_READING_PAUSES;
						}
						else
						{
							// complete
							_read_type = OOK_READER_COMPLETED;
						}
					}
				}
				else if (_read_type == OOK_READER_READING_PAUSES)
				{
					// pause doesnt save anything in the bit

					_pause_bit_count++;

					// if pause is completed, jump to the next fragment
					if (_pause_bit_count == pause_total)
					{
						_fragment_bit_count = 0;
						_read_type = OOK_READER_READING_FRAGMENT;
						_repeat_bit_count++;
					}
				}

				if (turn_bit_on)
				{
					// toggle on the bit on the byte var
					byte ^= 1UL << (bit - 1);
				}
			}
			bytes_read++;
		}

		rbuff[byte_i] = byte;
	}

	return {static_cast<size_t>(bytes_read)};
}