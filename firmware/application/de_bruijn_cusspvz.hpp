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

#include <vector>
#include <stdint.h>
#include "ch.h"

#ifndef __DE_BRUIJN_H__
#define __DE_BRUIJN_H__

class DeBruijnSequencer
{
public:
	DeBruijnSequencer(const uint8_t wordlength);
	~DeBruijnSequencer();

	DeBruijnSequencer(const DeBruijnSequencer &) = delete;
	DeBruijnSequencer(DeBruijnSequencer &&) = delete;
	DeBruijnSequencer &operator=(const DeBruijnSequencer &) = delete;
	DeBruijnSequencer &operator=(DeBruijnSequencer &&) = delete;

	size_t init(const uint8_t wordlength);
	void reset();
	bool read_bit();

	bool ended();
	size_t length();

	const uint8_t sequence_target_fill = 128;

private:
	bool _ended = false;
	size_t _length{0};
	const uint8_t k = 2; // radix
	uint8_t n = 8;		 // data length
	std::vector<bool> sequence;
	Thread *thread{nullptr};

	void db(uint8_t t, uint8_t p);
	bool a[101] = {};

	static msg_t static_fn(void *arg);
};

#endif /*__DE_BRUIJN_H__*/
