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

#ifndef __stream_writer_thread_H__
#define __stream_writer_thread_H__

#include "ch.h"

#include "event_m0.hpp"

#include "io.hpp"
#include "error.hpp"
#include "optional.hpp"

#include <cstdint>
#include <cstddef>
#include <utility>

class StreamWriterThread
{
public:
	StreamWriterThread(
		std::unique_ptr<stream::Writer> writer,
		size_t write_size,
		size_t buffer_count,
		std::function<void()> success_callback,
		std::function<void(Error)> error_callback);
	~StreamWriterThread();

	StreamWriterThread(const StreamWriterThread &) = delete;
	StreamWriterThread(StreamWriterThread &&) = delete;
	StreamWriterThread &operator=(const StreamWriterThread &) = delete;
	StreamWriterThread &operator=(StreamWriterThread &&) = delete;

	const StreamReceiveConfig &state() const
	{
		return config;
	}

private:
	StreamReceiveConfig config;
	std::unique_ptr<stream::Writer> writer;
	std::function<void()> success_callback;
	std::function<void(Error)> error_callback;
	Thread *thread{nullptr};

	static msg_t static_fn(void *arg);

	Optional<Error> run();
};

#endif /*__stream_writer_thread_H__*/
