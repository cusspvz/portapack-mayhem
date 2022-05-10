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

#include <vector>
#include <cstring>
#include <string>

#ifndef __ENCODERS_H__
#define __ENCODERS_H__

namespace encoders
{

#define ENC_TYPES_COUNT 18
#define ENCODER_UM3750 12
#define OOK_SAMPLERATE 2280000U
#define OOK_DEFAULT_STEP 8 // 70 kHz carrier frequency

	struct encoder_def_t
	{
		// Encoder details
		char name[16];		// Encoder chip ref/name
		uint8_t repeat_min; // Minimum repeat count (ignored on DeBruijn mode)

		// Word and Symbol format (Address, Data, Sync)
		uint8_t word_length;  // Total # of symbols (not counting sync)
		char word_format[32]; // A for Address, D for Data, S for sync

		// Symbol setup - Address + Data + Sync bit fragments
		char symfield_address_symbols[8];		 // List of possible symbols like "01", "01F"...
		char symfield_data_symbols[8];			 // Same as symfield_address_symbols
		uint8_t bit_fragments_length_per_symbol; // Indicates the length of the symbols_bit_fragmentss. Will be used to divide the symbol clock osc periods
		bool symbols_bit_fragments[4][20];		 // List of fragments for each symbol in previous *_symbols list order
		uint8_t sync_bit_length;				 // length of symbols_bit_fragments
		bool sync_bit_fragment[64];				 // Like symbols_bit_fragments

		// timing
		uint16_t shorter_pulse_period; // period on the shorter pulse (bit pulse rather than symbol pulse)
		// uint32_t default_clk_speed; // Default encoder clk frequency (often set by shitty resistor)
		uint16_t pause_bits; // Length of pause between repeats in symbols
	};

	size_t make_bitstream(std::string &fragments);
	void bitstream_append(size_t &bitstream_length, uint32_t bit_count, uint32_t bits);
	void generate_frame_fragments(std::vector<bool> *frame_fragments, const encoder_def_t *encoder_def, const uint8_t selected_symbol_indexes[32], const bool reversed);
	uint32_t get_frame_fragments_size(const encoder_def_t *encoder_def);

	// Warning ! If this is changed, make sure that ENCODER_UM3750 is still valid !
	constexpr encoder_def_t encoder_defs[ENC_TYPES_COUNT] = {

		// generic 8-bit encoder
		{
			// Encoder details
			"8-bits", // name
			50,		  // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			8,			// word_length
			"AAAAAAAA", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			4,	  // bit_fragments_length_per_symbol
			{
				{1, 0, 0, 0},
				{1, 1, 1, 0},
			},	// symbols_bit_fragments
			0,	// sync_bit_length
			{}, // sync_bit_fragment

			// Speed and clocks
			8, // shorter_pulse_period
			// 25000, // default_clk_speed
			0, // pause_bits
		},

		// generic 16-bit encoder
		{
			// Encoder details
			"16-bits", // name
			50,		   // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			16,					// word_length
			"AAAAAAAAAAAAAAAA", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			4,	  // bit_fragments_length_per_symbol
			{
				{1, 0, 0, 0},
				{1, 1, 1, 0},
			},	// symbols_bit_fragments
			0,	// sync_bit_length
			{}, // sync_bit_fragment

			// Speed and clocks
			8, // shorter_pulse_period
			// 25000, // default_clk_speed
			0, // pause_bits
		},

		// Test OOK Doorbell
		{
			// Encoder details
			"Doorbel", // name
			32,		   // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			24,							// word_length
			"AAAAAAAAAAAAAAAAAAAAAAAA", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			4,	  // bit_fragments_length_per_symbol
			{
				{1, 0, 0, 0},
				{1, 1, 1, 0},
			},	// symbols_bit_fragments
			0,	// sync_bit_length
			{}, // sync_bit_fragment

			// Speed and clocks
			57, // shorter_pulse_period
			// 141260, // default_clk_speed
			32 * 4, // pause_bits
		},

		// Test OOK Garage Door
		{
			// Encoder details
			"OH200DC", // name
			8,		   // repeat_min=230, looks like 8 is still working

			// Word and Symbol format (Address, Data, Sync)
			8,			// word_length
			"AAAAAAAA", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			8,	  // bit_fragments_length_per_symbol
			{
				{1, 1, 1, 1, 0, 0, 0, 0},
				{1, 0, 0, 0, 0, 0, 0, 0},
			},	// symbols_bit_fragments
			0,	// sync_bit_length
			{}, // sync_bit_fragment

			// Speed and clocks
			115, // shorter_pulse_period
			// 285000, // default_clk_speed
			70 * 8, // pause_bits
		},

		// PT2260-R2
		{
			// Encoder details
			"2260-R2", // name
			2,		   // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			12,				 // word_length
			"AAAAAAAAAADDS", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01F", // symfield_address_symbols
			"01",  // symfield_data_symbols
			8,	   // bit_fragments_length_per_symbol
			{
				{1, 0, 0, 0, 1, 0, 0, 0},
				{1, 1, 1, 0, 1, 1, 1, 0},
				{1, 0, 0, 0, 1, 1, 1, 0},
			},																								  // symbols_bit_fragments
			32,																								  // sync_bit_length
			{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // sync_bit_fragment

			// Speed and clocks
			128, // shorter_pulse_period
			// 150000, // default_clk_speed
			0, // pause_bits
		},

		// PT2260-R4
		{
			// Encoder details
			"2260-R4", // name
			2,		   // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			12,				 // word_length
			"AAAAAAAADDDDS", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01F", // symfield_address_symbols
			"01",  // symfield_data_symbols
			8,	   // bit_fragments_length_per_symbol
			{
				{1, 0, 0, 1, 0, 0, 0},
				{1, 1, 1, 0, 1, 1, 1, 0},
				{1, 0, 0, 0, 1, 1, 1, 0},
			},																								  // symbols_bit_fragments
			32,																								  // sync_bit_length
			{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // sync_bit_fragment

			// Speed and clocks
			128, // shorter_pulse_period
			// 150000, // default_clk_speed
			0, // pause_bits
		},

		// PT2262
		{
			// Encoder details
			"2262", // name
			4,		// repeat_min

			// Word and Symbol format (Address, Data, Sync)
			12,				 // word_length
			"AAAAAAAAAAAAS", // // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01F", // symfield_address_symbols
			"01F", // symfield_data_symbols
			8,	   // bit_fragments_length_per_symbol
			{
				{1, 0, 0, 0, 1, 0, 0, 0},
				{1, 1, 1, 0, 1, 1, 1, 0},
				{1, 0, 0, 0, 1, 1, 1, 0},
			},																								  // symbols_bit_fragments
			32,																								  // sync_bit_length
			{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // sync_bit_fragment

			// Speed and clocks
			4, // shorter_pulse_period
			// 20000, // default_clk_speed
			0, // pause_bits
		},

		// 16-bit ?
		{
			// Encoder details
			"16-bit", // name
			50,		  // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			16,					 // word_length
			"AAAAAAAAAAAAAAAAS", // // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			4,	  // bit_fragments_length_per_symbol
			{
				{1, 1, 1, 0},
				{1, 0, 0, 0},
			},																 // symbols_bit_fragments
			21,																 // sync_bit_length
			{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // sync_bit_fragment

			// Speed and clocks
			8, // shorter_pulse_period
			// 25000, // default_clk_speed
			0, // pause_bits
		},

		// RT1527
		{
			// Encoder details
			"1527", // name
			4,		// repeat_min

			// Word and Symbol format (Address, Data, Sync)
			24,							 // word_length
			"SAAAAAAAAAAAAAAAAAAAADDDD", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			4,	  // bit_fragments_length_per_symbol
			{
				{1, 0, 0, 0},
				{1, 1, 1, 0},
			},																								  // symbols_bit_fragments
			32,																								  // sync_bit_length
			{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // sync_bit_fragment

			// Speed and clocks
			32, // shorter_pulse_period
			// 100000, // default_clk_speed
			10 * 4, // pause_bits
		},

		// HK526E
		{
			// Encoder details
			"526E", // name
			4,		// repeat_min

			// Word and Symbol format (Address, Data, Sync)
			12,				// word_length
			"AAAAAAAAAAAA", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			3,	  // bit_fragments_length_per_symbol
			{
				{1, 1, 0},
				{1, 0, 0},
			},	// symbols_bit_fragments
			0,	// sync_bit_length
			{}, // sync_bit_fragment

			// Speed and clocks
			8, // shorter_pulse_period
			// 20000, // default_clk_speed
			10 * 3, // pause_bits
		},

		// HT12E
		{
			// Encoder details
			"12E", // name
			4,	   // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			12,				 // word_length
			"SAAAAAAAADDDD", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			3,	  // bit_fragments_length_per_symbol
			{
				{0, 1, 1},
				{0, 0, 1},
			},																												 // symbols_bit_fragments
			37,																												 // sync_bit_length
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // sync_bit_fragment

			// Speed and clocks
			1, // shorter_pulse_period
			// 3000, // default_clk_speed
			10 * 3, // pause_bits
		},

		// VD5026 13 bits ?
		{
			// Encoder details
			"5026", // name
			4,		// repeat_min

			// Word and Symbol format (Address, Data, Sync)
			12,				 // word_length
			"SAAAAAAAAAAAA", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"0123", // symfield_address_symbols
			"0123", // symfield_data_symbols
			16,		// bit_fragments_length_per_symbol
			{
				{1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
				{1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0},
				{1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
				{1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0},
			},																																				  // symbols_bit_fragments
			48,																																				  // sync_bit_length // does it make sense? isnt it just a pause in between frames?
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // sync_bit_fragment // does it make sense? isnt it just a pause in between frames?

			// Speed and clocks
			8, // shorter_pulse_period
			// 100000, // default_clk_speed
			10 * 16, // pause_bits
		},

		// UM3750
		{
			// Encoder details
			"UM3750", // name
			4,		  // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			12,				 // word_length
			"SAAAAAAAAAAAA", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			3,	  // bit_fragments_length_per_symbol
			{
				{0, 1, 1},
				{0, 0, 1},
			},		   // symbols_bit_fragments
			3,		   // sync_bit_length
			{0, 0, 1}, // sync_bit_fragment

			// Encoder details

			// Word and Symbol format (Address, Data, Sync)
			// Speed and clocks
			32, // shorter_pulse_period
			// 100000, // default_clk_speed

			// Symbol setup - Address + Data + Sync bit fragments
			((3 * 12) - 6) * 3, // pause_bits Compensates for pause delay bug in proc_ooktx
								// TODO: need to revisit this config since I've refactored the proc_ooktx
		},

		// Speed and clocks
		// UM3758
		{
			// Encoder details
			"UM3758", // name
			4,		  // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			18,					   // word_length
			"SAAAAAAAAAADDDDDDDD", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01F", // symfield_address_symbols
			"01",  // symfield_data_symbols
			6,	   // bit_fragments_length_per_symbol
			{
				{0, 1, 1, 0, 1, 1},
				{0, 0, 1, 0, 0, 1},
				{0, 1, 1, 0, 0, 1},
			},	 // symbols_bit_fragments
			1,	 // sync_bit_length
			{1}, // sync_bit_fragment

			// Speed and clocks
			16, // shorter_pulse_period
			// 160000, // default_clk_speed
			10, // pause_bits
		},

		// BA5104
		{
			// Encoder details
			"BA5104", // name
			4,		  // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			9,			  // word_length
			"SDDAAAAAAA", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			4,	  // bit_fragments_length_per_symbol
			{
				{1, 0, 0, 0},
				{1, 1, 1, 0},
			},	// symbols_bit_fragments
			0,	// sync_bit_length
			{}, // sync_bit_fragment

			// Speed and clocks
			768, // shorter_pulse_period
			// 455000, // default_clk_speed
			10 * 4, // pause_bits
		},

		// MC145026
		{
			// Encoder details
			"145026", // name
			2,		  // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			9,			  // word_length
			"SAAAAADDDD", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01F", // symfield_address_symbols
			"01",  // symfield_data_symbols
			16,	   // bit_fragments_length_per_symbol
			{
				{0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1},
				{0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0},
				{0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0},
			},														// symbols_bit_fragments
			18,														// sync_bit_length
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // sync_bit_fragment

			// Speed and clocks
			1, // shorter_pulse_period
			// 455000, // default_clk_speed
			2 * 16, // pause_bits
		},

		// HT6*** TODO: Add individual variations
		{
			// Encoder details
			"HT6***", // name
			3,		  // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			18,					   // word_length
			"SAAAAAAAAAAAADDDDDD", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01F", // symfield_address_symbols
			"01",  // symfield_data_symbols
			6,	   // bit_fragments_length_per_symbol
			{
				{0, 1, 1, 0, 1, 1},
				{0, 0, 1, 0, 0, 1},
				{0, 0, 1, 0, 1, 1},
			}, // symbols_bit_fragments
			// "0000000000000000000000000000000000001011001011001",
			49,																																					 // sync_bit_length
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1}, // sync_bit_fragment

			// Speed and clocks
			33, // shorter_pulse_period
			// 80000, // default_clk_speed
			10 * 6, // pause_bits
		},

		// TC9148
		{
			// Encoder details
			"TC9148", // name
			3,		  // repeat_min

			// Word and Symbol format (Address, Data, Sync)
			12,				// word_length
			"AAAAAAAAAAAA", // word_format

			// Symbol setup - Address + Data + Sync bit fragments
			"01", // symfield_address_symbols
			"01", // symfield_data_symbols
			4,	  // bit_fragments_length_per_symbol
			{
				{1, 0, 0, 0},
				{1, 1, 1, 0},
			},	// symbols_bit_fragments
			0,	// sync_bit_length
			{}, // sync_bit_fragment

			// Speed and clocks
			12, // shorter_pulse_period
			// 455000, // default_clk_speed
			10 * 4, // pause_bits
		},
	};

} /* namespace encoders */

#endif /*__ENCODERS_H__*/
