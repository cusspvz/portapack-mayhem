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

#ifndef __stream_reader_thread_H__
#define __stream_reader_thread_H__

#include "ch.h"

#include "event_m0.hpp"

#include "io.hpp"
#include "optional.hpp"

#include <cstdint>
#include <cstddef>
#include <utility>

class StreamReaderThread
{
public:
	StreamReaderThread(
		std::unique_ptr<stream::Reader> reader,
		size_t read_size,
		size_t buffer_count
	);
	~StreamReaderThread();

	StreamReaderThread(const StreamReaderThread &) = delete;
	StreamReaderThread(StreamReaderThread &&) = delete;
	StreamReaderThread &operator=(const StreamReaderThread &) = delete;
	StreamReaderThread &operator=(StreamReaderThread &&) = delete;

	const StreamTransmitConfig &state() const
	{
		return config;
	};

	enum StreamReaderThread_return
	{
		READ_ERROR = 0,
		END_OF_STREAM,
		TERMINATED
	};

private:
	StreamTransmitConfig config;
	std::unique_ptr<stream::Reader> reader;
	bool ready_sig;
	std::function<void(uint32_t return_code)> terminate_callback;
	Thread *thread{nullptr};

	MessageHandlerRegistration message_handler_fifo_signal{
		Message::ID::RequestSignal,
		[this](const Message *const p)
		{
			const auto message = static_cast<const RequestSignalMessage *>(p);
			if (message->signal == RequestSignalMessage::Signal::FillRequest)
			{
				this->ready_sig = true;
			}
		}};

	static msg_t static_fn(void *arg);

	uint32_t run();
};

#endif /*__stream_reader_thread_H__*/
