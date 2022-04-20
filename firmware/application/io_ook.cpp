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

uint64_t OOKReader::length()
{
	return (fragments.length() + pause_total) * repeat_total;
};

Result<uint64_t, OOKReaderError> OOKReader::read(void *const buffer, const uint64_t bytes)
{
	// assuming 8bit buffer array
	// TODO: we might need to measure the bit size of the buffer
	uint64_t bytes_read = 0;
	uint8_t *rbuff = (uint8_t *)buffer;

	// start filling the buffer
	for (size_t byte_i = 0; byte_i < sizeof(rbuff); byte_i++)
	{
		uint8_t byte = 0;

		if (read_type != OOK_READER_COMPLETED)
		{
			for (uint8_t bit = 8; bit > 0; bit--)
			{
				bool turn_bit_on = false;

				if (read_type == OOK_READER_READING_FRAGMENT)
				{
					turn_bit_on = fragments[fragment_bit_count] == '1';

					fragment_bit_count++;

					// if completed, jump to either a pause or completed
					if (fragment_bit_count == fragments.length())
					{
						if (repeat_bit_count < repeat_total)
						{
							// start pause
							pause_bit_count = 0;
							read_type = OOK_READER_READING_PAUSES;
						}
						else
						{
							// complete
							read_type = OOK_READER_COMPLETED;
						}
					}
				}
				else if (read_type == OOK_READER_READING_PAUSES)
				{
					// pause doesnt save anything in the bit

					pause_bit_count++;

					// if pause is completed, jump to the next fragment
					if (pause_bit_count == pause_total)
					{
						fragment_bit_count = 0;
						read_type = OOK_READER_READING_FRAGMENT;
						repeat_bit_count++;
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

	return bytes_read;
}