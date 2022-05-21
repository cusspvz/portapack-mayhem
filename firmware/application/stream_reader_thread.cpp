/*
 * Copyright (C) 2016 Jared Boone, ShareBrained Technology, Inc.
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

#include "stream_reader_thread.hpp"

#include "baseband_api.hpp"
#include "buffer_exchange.hpp"

struct BasebandTransmit
{
	BasebandTransmit(StreamTransmitConfig *const config)
	{
		baseband::stream_transmit_start(config);
	}

	~BasebandTransmit()
	{
		baseband::stream_transmit_stop();
	}
};

// StreamReaderThread ///////////////////////////////////////////////////////////

StreamReaderThread::StreamReaderThread(
	std::unique_ptr<stream::Reader> reader,
	size_t read_size,
	size_t buffer_count) : config{read_size, buffer_count}, reader{std::move(reader)}
{
	// Need significant stack for FATFS
	thread = chThdCreateFromHeap(NULL, 1024, NORMALPRIO + 10, StreamReaderThread::static_fn, this);
}

StreamReaderThread::~StreamReaderThread()
{
	ready_sig = false;

	if (thread)
	{
		chThdTerminate(thread);
		chThdWait(thread);

		thread = nullptr;
	}
}

msg_t StreamReaderThread::static_fn(void *arg)
{
	auto obj = static_cast<StreamReaderThread *>(arg);
	const uint32_t return_code = obj->run();

	StreamReaderThreadDoneMessage message{return_code};
	EventDispatcher::send_message(message);

	return 0;
}

uint32_t StreamReaderThread::run()
{
	BasebandTransmit replay{&config};
	BufferExchange buffers{&config};

	StreamBuffer *prefill_buffer{nullptr};

	// Wait for FIFOs to be allocated in baseband
	while (!ready_sig)
	{
		chSysLock();
		chSchGoSleepTimeoutS(THD_STATE_SUSPENDED, 100);
		chSysUnlock();
	};

	// While empty buffers fifo is not empty...
	while (!buffers.empty())
	{
		if (chThdShouldTerminate())
			break;

		prefill_buffer = buffers.get_prefill();

		if (prefill_buffer == nullptr)
		{
			buffers.put_app(prefill_buffer);
		}
		else
		{
			size_t blocks = config.read_size / 512;
			// uint64_t total_read = 0;

			for (size_t c = 0; c < blocks; c++)
			{
				auto read_result = reader->read(&((uint8_t *)prefill_buffer->data())[c * 512], 512);
				if (read_result.is_error())
				{
					return READ_ERROR;
				}

				// total_read += read_result.value();
			}

			prefill_buffer->set_size(config.read_size);
			// prefill_buffer->set_size(total_read);

			buffers.put(prefill_buffer);
		}
	};

	baseband::set_fifo_data(nullptr);

	while (!chThdShouldTerminate())
	{
		auto buffer = buffers.get();

		if (chThdShouldTerminate())
			break;

		auto read_result = reader->read(buffer->data(), buffer->capacity());

		if (chThdShouldTerminate())
			break;

		if (read_result.is_error())
		{
			return READ_ERROR;
		}
		else
		{
			if (read_result.value() == 0)
			{
				return END_OF_STREAM;
			}
		}

		buffer->set_size(buffer->capacity());
		// buffer->set_size(read_result.value());

		buffers.put(buffer);
	}

	return TERMINATED;
}