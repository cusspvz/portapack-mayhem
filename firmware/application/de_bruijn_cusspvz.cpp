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

#include "de_bruijn_cusspvz.hpp"
#include <math.h> /* pow */

// DeBruijnSequencer ///////////////////////////////////////////////////////////

DeBruijnSequencer::DeBruijnSequencer(const uint8_t wordlength)
{
	// reserve memory
	initial_sequence_cache.reserve(sequence_target_fill);
	_seq_buffer.reserve(sequence_target_fill);

	// thread = chThdCreateFromHeap(NULL, 1024, NORMALPRIO + 10, DeBruijnSequencer::static_fn, this);

	init(wordlength);
};

DeBruijnSequencer::~DeBruijnSequencer()
{
	if (thread)
	{
		if (thread->p_state != THD_STATE_FINAL)
		{
			chThdTerminate(thread);
			chThdWait(thread);
		}

		thread = nullptr;
	}
}

uint64_t DeBruijnSequencer::length()
{
	return _length;
};

bool DeBruijnSequencer::consumed()
{
	return thread_ended() && _seq_buffer.size() == 0;
}

bool DeBruijnSequencer::thread_ended()
{
	return _generated_length >= _length;
};

uint64_t DeBruijnSequencer::init(uint8_t wordlength)
{
	n = wordlength;

	// calculate new length
	_length = pow(k, n) + (n - 1);
	reset();

	// TODO: this shouldnt be here, but on a thread instead. just using for testing purposes
	// db(1, 1);

	// Reset the thread

	return _length;
};

void DeBruijnSequencer::reset()
{
	// fill the var `a` with 0s
	for (uint8_t i = 1; i <= sizeof(a); i++)
		a[i] = false;

	_seq_buffer.clear();
	initial_sequence_cache.clear();
};

msg_t DeBruijnSequencer::static_fn(void *arg)
{
	DeBruijnSequencer *seq = static_cast<DeBruijnSequencer *>(arg);
	seq->run();
	return 0;
};

void DeBruijnSequencer::run()
{
	// TODO: consider reusing the heap

	db(1, 1);

	if (chThdShouldTerminate())
	{
		for (uint8_t i = 0, nremain = n - 1; nremain > 0; i += 2, nremain--)
		{
			_seq_buffer.push_back(initial_sequence_cache[i % _generated_length]);
			_generated_length++;
		}
	}
}

void DeBruijnSequencer::db(uint8_t t, uint8_t p)
{
	if (flow_control())
		return;

	if (t > n)
	{
		if (n % p == 0)
			for (uint32_t j = 1; j <= p; j++)
			{
				_seq_buffer.push_back(a[j]);
				_generated_length++;

				if (initial_sequence_cache.size() < sequence_target_fill)
				{
					initial_sequence_cache.push_back(a[j]);
				}
			}

		return;
	}

	if (flow_control())
		return;

	a[t] = a[t - p];
	db(t + 1, p);
	for (uint8_t j = a[t - p] + 1; j < k; j++)
	{

		if (flow_control())
			return;

		a[t] = j;
		db(t + 1, t);
	}
};

bool DeBruijnSequencer::flow_control()
{
	// if we've reached our target size, lets wait a bit until it gets consumed
	while (_seq_buffer.size() >= sequence_target_fill)
	{
		chThdYield();
	};

	return chThdShouldTerminate();
}

bool DeBruijnSequencer::read_bit()
{
	bool cur_bit = _seq_buffer[0];

	// delete the first entry from the _seq_buffer
	_seq_buffer.erase(_seq_buffer.begin());

	bits_read++;

	return cur_bit;
};
