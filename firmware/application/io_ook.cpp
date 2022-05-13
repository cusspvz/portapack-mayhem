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
#include <bitset>

// OOKFrameReader

uint64_t OOKFrameReader::length()
{
	uint64_t frame_length = frame_fragments->size() * repetitions_cursor.total / 8;
	uint64_t pauses_length = pauses_cursor.total * (repetitions_cursor.total - (!completition_requires_pause ? 1 : 0)) / 8;

	return frame_length + pauses_length;
};

void OOKFrameReader::reset()
{
	bytes_read = 0;

	// repetition and pauses
	repetitions_cursor.start_over();
	pauses_cursor.start_over();
	fragments_cursor.start_over();
	fragments_cursor.total = frame_fragments->size();

	// ongoing read vars
	read_type = OOK_READER_READING_FRAGMENT;
}

void OOKFrameReader::change_read_type(OOKFrameReaderReadType rt)
{

	// At this point we'll evaluate if we've completed or not
	if (repetitions_cursor.is_done())
	{

		if (
			!completition_requires_pause ||
			pauses_cursor.is_done())
		{
			// completed
			read_type = OOK_READER_COMPLETED;

			if (on_complete)
			{
				on_complete(*this);
			}

			return;
		}
	}

	// in a normal scenario, we'll reset counters and change the read type
	fragments_cursor.start_over();
	pauses_cursor.start_over();
	read_type = rt;
};

Result<uint64_t, Error> OOKFrameReader::read(void *const buffer, const uint64_t bsize)
{
	uint64_t bread = 0;
	std::bitset<32> *rbuff = (std::bitset<32> *)buffer;

	// start filling the buffer
	for (uint64_t rbi = 0; rbi < bsize; rbi++)
	{
		rbuff[rbi].reset();

		if (read_type != OOK_READER_COMPLETED)
		{
			for (uint8_t bit = 0; bit < 32; bit++)
			{

				switch (read_type)
				{
				case OOK_READER_COMPLETED:
					continue;

				case OOK_READER_READING_PAUSES:
					// pause doesnt save anything in the bit
					pauses_cursor.index++;

					// if pause is completed, jump to the next fragment
					if (pauses_cursor.is_done())
						change_read_type(OOK_READER_READING_FRAGMENT);
					break;

				case OOK_READER_READING_FRAGMENT:

					// if (fragments_cursor.index == 0 && on_before_frame_fragment_usage)
					// {
					// 	// Allow the app to generate the fragment on demand, or at a single shot
					// 	on_before_frame_fragment_usage(*this);
					// };

					rbuff[rbi].set(bit, frame_fragments->at(fragments_cursor.index));

					fragments_cursor.index++;
					if (fragments_cursor.is_done())
					{
						repetitions_cursor.index++;
						change_read_type(OOK_READER_READING_PAUSES);
					}

					break;
				}
			}

			bread += 4;
		}
	}

	return Result<uint64_t, Error>(bread);
}

// OOKDebruijnReader

uint64_t OOKDebruijnReader::length()
{
	return 2048;
	// return sequencer->length() / 8 * fragments_cursor.total;
};

void OOKDebruijnReader::reset()
{
	fragments_cursor.total = on_symbol_fragments->size();
	fragments_cursor.start_over();
};

Result<uint64_t, Error> OOKDebruijnReader::read(void *const buffer, const uint64_t bsize)
{
	uint64_t bread = 0;
	std::bitset<32> *rbuff = (std::bitset<32> *)buffer;

	// start filling the buffer
	for (uint64_t rbi = 0; rbi < bsize; rbi++)
	{
		rbuff[rbi].reset();

		if (read_type != OOK_DEBRUIJN_COMPLETED)
		{
			for (uint8_t bit = 0; bit < 32; bit++)
			{
				if (read_type == OOK_DEBRUIJN_READING_BIT)
				{
					// read bit from the debruijn thread and switch to the correct read type
					// cur_bit = sequencer->read_bit();
					cur_bit = !cur_bit;

					fragments_cursor.start_over();
					read_type = OOK_DEBRUIJN_READING_SYMBOL_FRAGMENT;
				}

				if (read_type == OOK_DEBRUIJN_READING_SYMBOL_FRAGMENT)
				{
					// read from the on fragments
					rbuff[rbi].set(bit, (cur_bit ? on_symbol_fragments->at(fragments_cursor.index) : off_symbol_fragments->at(fragments_cursor.index)));

					// if we've reached the end of the fragment
					fragments_cursor.index++;

					if (fragments_cursor.is_done())
					{
						// TODO in case the debruijn is complete, lets wrap it up, otherwise, read next bit
						// read_type = sequencer->consumed() ? OOK_DEBRUIJN_COMPLETED : OOK_DEBRUIJN_READING_BIT;
						read_type = false ? OOK_DEBRUIJN_COMPLETED : OOK_DEBRUIJN_READING_BIT;
					}
				}
			}

			bread += 4;
		}
	}

	return Result<uint64_t, Error>(bread);
};