/*
 * Copyright (C) 2014 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
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

#include "proc_ooktx.hpp"
#include "portapack_shared_memory.hpp"
#include "sine_table_int8.hpp"
#include "event_m4.hpp"
#include "utility.hpp"
#include <cstdint>

void OOKTxProcessor::execute(const buffer_c8_t &buffer)
{

	if (!configured)
		return;

	// This is called at 2.28M/2048 = 1113Hz
	// Calculate the amount of bits this will need to read from the memory
	int8_t re, im;

	for (size_t i = 0; i < buffer.count; i++)
	{
		// check what the processor is going to emit next
		if (configured)
		{

			// At this point we want to ensure the cur_bit is continuously sampled through the baseband
			// sampling buffer so we can print a set of sinewaves (also called short-pulse) representing the bit
			if (!bit_sampling.is_done())
			{
				bit_sampling.index++;
			}
			else
			{
				// at this point, we want to transmit a new bit
				bit_sampling.index = 0;

				// - if the sample is fully consumed or empty, attempt to read from the stream
				if (bit_cursor.is_last())
				{
					//   - if the stream is empty, it means we're done! :yey:
					if (fill_buffer() == 0)
						done();

					bit_cursor.index = 0;
				}
				else
				{
					// - if our internal sample still has bits, gather the next bit
					bit_cursor.index++;
				}

				// current_bit = bit_buffer[OOK_BIT_BUFFER_SIZE - bit_cursor.index];
				current_bit = !current_bit;
			}
		}

		// Handle the sine wave depending if the current bit is on or off
		if (current_bit)
		{
			phase += 1024000; // phasing was too long, it was set to 200
			sphase = phase + (64 << 18);

			re = (sine_table_i8[(sphase & 0x03FC0000) >> 18]);
			im = (sine_table_i8[(phase & 0x03FC0000) >> 18]);
		}
		else
		{
			re = 0;
			im = 0;
		}

		buffer.p[i] = {re, im};
	}

	// inform UI about the progress if it still is confifured
	if (configured)
	{
		txprogress_message.progress = stream->bytes_read;
		shared_memory.application_queue.push(txprogress_message);
	}
}

size_t OOKTxProcessor::fill_buffer()
{
	// // test by filling the bit_buffer with the same data up until 1024 bytees
	// if (stream->bytes_read >= 1024)
	// 	return 0;

	// uint32_t test = 0b10101111101011111010111100000000;

	// auto count = 4;
	// memcpy(&bit_buffer, &test, count);
	// stream->bytes_read += count;
	// bit_cursor.total = count * 8;

	// return count;

	if (!stream)
		return 0;

	auto read_size = stream->read(&bit_buffer, sizeof(bit_buffer));

	if (read_size.is_error() || read_size.value() == 0)
		return 0;

	// setup the maximum amount of bits in the bit_cursor
	bit_cursor.total = read_size.value() * 8;

	return read_size.value();
}

void OOKTxProcessor::done()
{
	// Transmission is now completed
	txprogress_message.progress = stream->bytes_read;
	txprogress_message.done = true;
	shared_memory.application_queue.push(txprogress_message);

	reset();
}

void OOKTxProcessor::reset()
{
	configured = false;

	if (stream)
		stream.reset();

	// clear buffer
	bit_buffer.reset();
	current_bit = false;

	txprogress_message.progress = 0;
	txprogress_message.done = false;

	bit_sampling.index = 0;
	bit_cursor.index = 0;
	bit_cursor.total = sizeof(bit_buffer) * 8;
}

void OOKTxProcessor::on_message(const Message *const message)
{
	switch (message->id)
	{
	case Message::ID::OOKConfigure:
		ook_config(reinterpret_cast<const OOKConfigureMessage *>(message));
		break;

	case Message::ID::StreamDataExchangeConfigure:
		stream_config(reinterpret_cast<const StreamDataExchangeMessage *>(message));
		break;

	default:
		break;
	}
}

void OOKTxProcessor::ook_config(const OOKConfigureMessage *message)
{
	bit_sampling.total = baseband_fs / (1000000 / message->pulses_per_bit);
	reset();
};
void OOKTxProcessor::stream_config(const StreamDataExchangeMessage *message)
{
	if (!message->config)
		return;

	if (stream)
		stream.reset();

	stream = std::make_unique<StreamDataExchange>(message->config);

	fill_buffer();

	bit_sampling.index = 0;
	bit_cursor.index = 0;

	configured = true;
};

int main()
{
	EventDispatcher event_dispatcher{std::make_unique<OOKTxProcessor>()};
	event_dispatcher.run();
	return 0;
}
