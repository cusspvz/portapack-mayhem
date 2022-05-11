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
	init(wordlength);

	// TODO: this shouldnt be here, but on a thread instead. just using for testing purposes
	db(1, 1);

	// Need significant stack for FATFS
	// thread = chThdCreateFromHeap(NULL, 512, NORMALPRIO + 10, DeBruijnSequencer::static_fn, this);
};

DeBruijnSequencer::~DeBruijnSequencer()
{
	_ended = true;

	if (thread)
	{
		chThdTerminate(thread);
		chThdWait(thread);
		thread = nullptr;
	}
}

size_t DeBruijnSequencer::length()
{
	return _length;
};
bool DeBruijnSequencer::ended()
{
	return _ended;
};

size_t DeBruijnSequencer::init(uint8_t wordlength)
{
	n = wordlength;

	// calculate new length
	_length = pow(k, n) + (n - 1);
	reset();

	return _length;
};

void DeBruijnSequencer::reset()
{
	// fill the var `a` with 0s
	for (uint8_t i = 1; i <= sizeof(a); i++)
		a[i] = false;

	_ended = false;
	sequence.clear();
};

msg_t DeBruijnSequencer::static_fn(void *arg)
{
	auto sequencer = static_cast<DeBruijnSequencer *>(arg);
	// sequencer->db(1, 1);

	// TODO: analyse whats the best way to achieve this on a streamed approach
	// for (uint8_t i = 0, nremain = n - 1; nremain > 0; i += 2, nremain--)
	// 	sequence.push_back(sequence[i % sequence.length()]);

	return 0;
};

void DeBruijnSequencer::db(uint8_t t, uint8_t p)
{
	if (chThdShouldTerminate())
		return;

	// // if we've reached our target size, lets wait a bit until it gets consumed
	// while (sequence.size() >= sequence_target_fill)
	// {
	// 	chThdSleep(1000);
	// };

	if (t > n)
	{
		if (n % p == 0)
			for (uint32_t j = 1; j <= p; j++)
				sequence.push_back(a[j]);

		return;
	}

	a[t] = a[t - p];
	db(t + 1, p);
	for (uint8_t j = a[t - p] + 1; j < k; j++)
	{
		a[t] = j;
		db(t + 1, t);
	}
};

bool DeBruijnSequencer::read_bit()
{
	bool cur_bit = sequence[0];

	// delete from the sequence
	sequence.erase(sequence.begin());

	return cur_bit;
};
