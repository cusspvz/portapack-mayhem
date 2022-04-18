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
	int8_t re, im;

	// This is called at 2.28M/2048 = 1113Hz

	if (!configured)
		return;

	// We're going to treat the stream as a bitstream for the fragment's
	// samples that this has to transmit. For this reason, we'll read only the
	// necessary for this current buffer handling.
	// That would be: 2048 samples / 8 / samples_per_bit
	// Note: 2048 samples = sizeof(*buffer.p) = sizeof(C8) = 2*int8 = 2 bytes // buffer.count = 2048
	if (stream)
	{
		const size_t bytes_to_read = buffer.count / 8 / 10 / samples_per_bit;
		bytes_read += stream->read(_buffer.p, bytes_to_read);
	}

	for (size_t i = 0; i < buffer.count; i++)
	{
		// don't do nothing else in case we stop the transmission
		if (configured)
		{

			// Synthesis at 2.28M/SAMPLES_PER_BIT {10} = 228kHz
			if (s)
				// don't change the cur_bit if we're still handling the rest of the bit's samples
				s--;
			else
			{
				s = 10 - 1;

				// Samples per bit control
				if (sample_count >= samples_per_bit)
				{
					// Prepare to gather the next bit
					// or end in case we hit the ceilling
					if (bit_pos >= bitstream_length)
					{
						// Transmission is now completed
						cur_bit = 0;
						txprogress_message.done = true;
						shared_memory.application_queue.push(txprogress_message);
						configured = false;
					}
					else
					{
						// Get next bit
						cur_bit = (_buffer.p[bit_pos >> 3] << (bit_pos & 7)) & 0x80;
						bit_pos++;
					}

					sample_count = 0;
				}
				else
				{
					sample_count++;
				}
			}
		}

		// Handle the sine wave depending if the current bit is on or off
		if (cur_bit)
		{
			phase = (phase + 200); // What ?
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
}

void OOKTxProcessor::on_message(const Message *const p)
{
	const auto ook_message = *reinterpret_cast<const OOKConfigureMessage *>(p);
	const auto stream_message = *reinterpret_cast<const StreamConfigMessage *>(p);

	switch (p->id)
	{
	case Message::ID::OOKConfigure:
		bitstream_length = ook_message.bitstream_length;
		samples_per_bit = ook_message.samples_per_bit / 10;
		break;

	case Message::ID::StreamConfig:
		configured = false;
		bytes_read = 0;

		if (stream_message.config)
		{
			stream = std::make_unique<StreamOutput>(stream_message.config);

			// Tell application that the buffers and FIFO pointers are ready, prefill
			shared_memory.application_queue.push(sig_message);
		}
		else
		{
			stream.reset();
		}
		break;

	// App has prefilled the buffers, we're ready to go now
	case Message::ID::FIFOData:
		s = 0;
		bit_pos = 0;
		cur_bit = 0;
		txprogress_message.progress = 0;
		txprogress_message.done = false;
		configured = true;
		break;

	default:
		break;
	}
}

int main()
{
	EventDispatcher event_dispatcher{std::make_unique<OOKTxProcessor>()};
	event_dispatcher.run();
	return 0;
}
