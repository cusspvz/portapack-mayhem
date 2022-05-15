/*
 * Copyright (C) 2022 José Moreira @cusspvz
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

#include <functional>
#include <deque>
#include <stdint.h>
#include "ch.h"

#ifndef __DE_BRUIJN_H__
#define __DE_BRUIJN_H__

const uint8_t DE_BRUIJN_BUFFER_SIZE = 64;

class DeBruijnSequencer
{
public:
	DeBruijnSequencer(const uint8_t wordlength);
	~DeBruijnSequencer();

	DeBruijnSequencer(const DeBruijnSequencer &) = delete;
	DeBruijnSequencer(DeBruijnSequencer &&) = delete;
	DeBruijnSequencer &operator=(const DeBruijnSequencer &) = delete;
	DeBruijnSequencer &operator=(DeBruijnSequencer &&) = delete;

	uint64_t init(const uint8_t wordlength);
	void reset();
	bool read_bit();
	bool consumed();

	void wait_for_buffer_completed();
	void thread_stop();
	bool thread_ended();
	uint64_t length();

	// This initial sequence cache serves to fill the UI waveform so the user
	// see the initial output of the sequencer (as we transform the output bits
	// into bool symbol fragments)
	std::vector<bool> waveform_cache{};

	const uint8_t k = 2; // radix (this is constant as I'm building a binary DeBruijn Sequencer)
	uint8_t n = 8;		 // word length

private:
	// uint8_t _consumer_index{0};	 // cursor for the consumer to navigate through the buffer
	// uint8_t _generator_index{0}; // cursor for the generator to navigate through the buffer

	// sequence fifo buffer, used by the generator and the consumer as this buffer has
	// a fixed-length. Using deque as std fifo ends up using it anyway
	std::deque<bool> _seq_buffer_queue{};

	// buffer semaphore
	void signal_buffer_completed();
	BinarySemaphore _bsem{};

	// length related
	uint64_t _consumed_length{0};  // count the amount of consumed chars (bits)
	uint64_t _generated_length{0}; // count the amount of generated chars (bits)
	uint64_t _target_length{0};	   // counts the target amoung of generated chars

	// debruijn recursive function
	void db(uint8_t t, uint8_t p);
	bool a[101] = {}; // buffer to save the sequenced pairs

	// sequencer thread
	Thread *thread{nullptr};
	void thread_start();
	static msg_t static_fn(void *arg);
	void run();
	bool flow_control();
};

#endif /*__DE_BRUIJN_H__*/
