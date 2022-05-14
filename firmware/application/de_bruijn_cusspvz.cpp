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
	// set sequence fifo buffer capacity
	waveform_cache.resize(DE_BRUIJN_BUFFER_SIZE);
	_seq_buffer_queue.resize(DE_BRUIJN_BUFFER_SIZE);

	// Start signal
	chBSemInit(&_bsem, false);

	init(wordlength);
};

DeBruijnSequencer::~DeBruijnSequencer()
{
	thread_stop();
};

uint64_t DeBruijnSequencer::length()
{
	return _target_length;
};

bool DeBruijnSequencer::consumed()
{
	return !thread && _seq_buffer_queue.empty();
};

void DeBruijnSequencer::thread_stop()
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
};

void DeBruijnSequencer::thread_start()
{
	thread_stop();

	thread = chThdCreateFromHeap(NULL, 1280, NORMALPRIO + 10, DeBruijnSequencer::static_fn, this);
};

bool DeBruijnSequencer::thread_ended()
{
	return !thread;
};

void DeBruijnSequencer::wait_for_buffer_completed() {
	if (!thread || _seq_buffer_queue.size() == DE_BRUIJN_BUFFER_SIZE) {
		return;
	}

	chBSemWait(&_bsem);

	// reset signal
	chBSemReset(&_bsem, false);
};

void DeBruijnSequencer::signal_buffer_completed() {
	chBSemSignal(&_bsem);
}

uint64_t DeBruijnSequencer::init(uint8_t wordlength)
{
	n = wordlength;

	// calculate new length
	_target_length = pow(k, n) + (n - 1);

	// fill the var `a` with 0s
	for (uint8_t i = 0; i < sizeof(a); i++)
		a[i] = false;

	// clear waveform cache
	waveform_cache.clear();

	// clear fifo queue
	_seq_buffer_queue.clear();

	// Reset the thread
	thread_start();

	return _target_length;
};

msg_t DeBruijnSequencer::static_fn(void *arg)
{
	// todo: might be good to use a chibi thread semaphore to avoid recreating threads

	DeBruijnSequencer *seq = static_cast<DeBruijnSequencer *>(arg);
	seq->run();
	return 0;
};

void DeBruijnSequencer::run()
{
	db(1, 1);

	for (uint8_t i = 0, nremain = n - 1; nremain > 0; i += 2, nremain--)
	{
		if (flow_control())
			return;

		_seq_buffer_queue.push_back(waveform_cache[i % _generated_length]);
		_generated_length++;
	}

	signal_buffer_completed();
}

void DeBruijnSequencer::db(uint8_t t, uint8_t p)
{
	if (chThdShouldTerminate())
		return;

	if (t > n)
	{
		if (n % p == 0)
			for (uint32_t j = 1; j <= p; j++)
			{
				if (flow_control())
					return;

				// Generate bit (a[j])
				if (waveform_cache.size() < DE_BRUIJN_BUFFER_SIZE)
					waveform_cache.push_back(a[j]);

				_seq_buffer_queue.push_back(a[j]);
				_generated_length++;
			}

		return;
	}

	if (chThdShouldTerminate())
		return;

	a[t] = a[t - p];
	db(t + 1, p);
	for (uint8_t j = a[t - p] + 1; j < k; j++)
	{

		if (chThdShouldTerminate())
			return;

		a[t] = j;
		db(t + 1, t);
	}
};

bool DeBruijnSequencer::flow_control()
{
	if (_seq_buffer_queue.size() == DE_BRUIJN_BUFFER_SIZE) {
		signal_buffer_completed();
	}

	// if we've reached our target size, lets wait a bit until it gets consumed
	while (_seq_buffer_queue.size() == DE_BRUIJN_BUFFER_SIZE)
	{
		// suspend thread with a timeout (200 cycles)
		chSysLock();
		chSchGoSleepTimeoutS(THD_STATE_SUSPENDED, 500);
		chSysUnlock();

		if (chThdShouldTerminate())
			return true;
	};

	return chThdShouldTerminate();
}

bool DeBruijnSequencer::read_bit()
{

	// resume but only if it strictly necessary
	// if (_seq_buffer_queue.empty())
	// {
	// 	chSchReadyI(thread);

		// if we were unable to fill more, always return false
		if (_seq_buffer_queue.empty())
		{
			return false;
		}
	// }

	// read and delete the first entry from the _seq_buffer_queue
	bool cur_bit = _seq_buffer_queue.front();
	_seq_buffer_queue.pop_front();
	_consumed_length++;
	return cur_bit;
};
