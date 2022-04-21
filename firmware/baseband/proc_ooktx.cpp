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

	for (size_t i = 0; i < buffer.count; i++)
	{
		// don't do nothing else in case we stop the transmission

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
				sample_count = 0;

				// get the next cur_bit
				if (configured)
				{
					process_cur_bit();
				}
			}
			else
			{
				sample_count++;
			}
		}

		// Handle the sine wave depending if the current bit is on or off
		if (cur_bit)
		{
			// shape the sine wave correctly during 10 cycles (10 * 1113Hz)
			phase = (phase + 200);
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
		txprogress_message.progress = bytes_read;
		shared_memory.application_queue.push(txprogress_message);
	}
}

void OOKTxProcessor::process_cur_bit()
{

	bit_pos--;

	if (bit_pos == 0)
	{
		bit_pos = 8;

		uint32_t bytes_streamed = 0;

		if (stream)
		{
			bytes_streamed = stream->read(&byte_sample, 1);
			bytes_read += bytes_streamed;
		}

		if (!stream || bytes_streamed == 0)
		{
			// Transmission is now completed
			cur_bit = 0;
			txprogress_message.done = true;
			shared_memory.application_queue.push(txprogress_message);
			configured = false;
			return;
		}
	}

	cur_bit = byte_sample & (1UL << (bit_pos - 1));
}

void OOKTxProcessor::on_message(const Message *const message)
{
	switch (message->id)
	{
	case Message::ID::OOKConfigure:
		ook_config(*reinterpret_cast<const OOKConfigureMessage *>(message));
		break;

	case Message::ID::StreamConfig:
		stream_config(*reinterpret_cast<const StreamConfigMessage *>(message));
		break;

	// App has prefilled the buffers, we're ready to go now
	case Message::ID::FIFOData:
		s = 0;
		bit_pos = 0;
		cur_bit = 0;
		txprogress_message.progress = 0;
		txprogress_message.done = false;
		configured = true;

		// share init progress
		shared_memory.application_queue.push(txprogress_message);
		break;

	default:
		break;
	}
}

void OOKTxProcessor::ook_config(const OOKConfigureMessage &message)
{
	samples_per_bit = message.samples_per_bit / 10;
};
void OOKTxProcessor::stream_config(const StreamConfigMessage &message)
{
	configured = false;
	bytes_read = 0;

	if (message.config)
	{
		stream = std::make_unique<StreamOutput>(message.config);

		// Tell application that the buffers and FIFO pointers are ready, prefill
		shared_memory.application_queue.push(sig_message);
	}
	else
	{
		stream.reset();
	}
};

int main()
{
	EventDispatcher event_dispatcher{std::make_unique<OOKTxProcessor>()};
	event_dispatcher.run();
	return 0;
}
