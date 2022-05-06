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

#ifndef __proc_ooktx_H__
#define __proc_ooktx_H__

#include "baseband_processor.hpp"
#include "baseband_thread.hpp"

#include "stream_output.hpp"
#include "cursor.hpp"

#include <array>
#include <memory>

class OOKTxProcessor : public BasebandProcessor
{
public:
	void execute(const buffer_c8_t &buffer) override;
	void process();
	void done();

	void on_message(const Message *const message) override;

private:
	static constexpr size_t baseband_fs = 2280000; // sample rate

	BasebandThread baseband_thread{baseband_fs, this, NORMALPRIO + 20, baseband::Direction::Transmit};

	bool configured{false};
	uint32_t samples_per_bit{0};

	// streaming approach
	uint32_t bytes_read{0};
	std::unique_ptr<StreamOutput> stream{};
	uint8_t bit_buffer_byte_size = 1;
	uint8_t bit_buffer{};
	cursor bit_cursor{};
	bool current_bit = false;

	// variables to help the processor execution routine
	cursor bit_sampling{};
	uint32_t phase{0}, sphase{0};

	void ook_config(const OOKConfigureMessage &message);
	void stream_config(const StreamConfigMessage &message);

	TXProgressMessage txprogress_message{};
	RequestSignalMessage sig_message{RequestSignalMessage::Signal::FillRequest};
};

#endif
