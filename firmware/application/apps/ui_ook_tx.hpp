/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
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

#include "ui.hpp"
#include "ui_tabview.hpp"
#include "ui_transmitter.hpp"
#include "de_bruijn_cusspvz.hpp"
#include "transmitter_model.hpp"
#include "encoders.hpp"
#include "io_file.hpp"
#include "io_ook.hpp"
#include "stream_reader_thread.hpp"

using namespace encoders;

namespace ui
{

	static constexpr Style style_success{
		.font = font::fixed_8x16,
		.background = Color::black(),
		.foreground = Color::green(),
	};
	static constexpr Style style_info{
		.font = font::fixed_8x16,
		.background = Color::black(),
		.foreground = Color::green(),
	};
	static constexpr Style style_err{
		.font = font::fixed_8x16,
		.background = Color::black(),
		.foreground = Color::red(),
	};

	enum tx_modes
	{
		TX_MODE_IDLE = 0,
		TX_MODE_MANUAL = 1,
		TX_MODE_DEBRUIJN = 2,
		TX_MODE_BRUTEFORCE = 3,
		TX_MODE_LOADER = 4,
	};

	const std::vector<std::pair<std::string, int32_t>> PERIODS_PER_BIT = {
		{"2", 2},
		{"3", 3},
		{"4", 4},
		{"8", 8},
		{"16", 16},
		{"24", 24},
		{"32", 32},
		{"48", 48},
		{"96", 96},
		{"128", 128},
		{"198", 198},
		{"228", 228},
		{"256", 256},
		{"512", 512},
		{"768", 768},
		{"920", 920},
		{"1024", 1024},
		{"1280", 1280},
		{"2048", 2048},
		{"4144", 4144},
	};

	///////////////////////////////////////////////////////////////////////////////
	// OOKTxGeneratorView

	class OOKTxGeneratorView : public View
	{
	public:
		// allow the main view to hook change events
		std::function<void()> on_waveform_change_request{};
		std::function<void(std::string)> on_status_change{};
		std::function<void()> on_encoder_change{};

		OOKTxGeneratorView(NavigationView &nav, Rect parent_rect);
		OOKTxGeneratorView(const OOKTxGeneratorView &) = delete;
		OOKTxGeneratorView(OOKTxGeneratorView &&) = delete;
		OOKTxGeneratorView &operator=(const OOKTxGeneratorView &) = delete;
		OOKTxGeneratorView &operator=(OOKTxGeneratorView &&) = delete;
		void focus() override;
		void on_show() override;

		uint16_t repeat_skip_bits_count();
		const encoder_def_t *encoder_def{};
		void reset_symfield();
		void check_if_encoder_is_vuln_to_debruijn();

		// UI related

		Labels labels{
			{{1 * 8, 0}, "Type:", Color::light_grey()},
			{{1 * 8, 2 * 8}, "TX:", Color::light_grey()},
			{{16 * 8, 0 * 8}, "Sh Pulse:", Color::light_grey()},
			{{28 * 8, 0 * 8}, "us", Color::light_grey()},
			{{1 * 8, 4 * 8}, "Repeat:", Color::light_grey()},
			{{1 * 8, 6 * 8}, "Pause:", Color::light_grey()},
			{{1 * 8, 8 * 8}, "Symbols:", Color::light_grey()},
		};

		OptionsField options_encoder{
			{7 * 8, 0},
			7,
			{
				// Options are loaded at runtime
			},
		};

		OptionsField options_tx_method{
			{5 * 8, 2 * 8},
			8,
			{
				{"Manual", TX_MODE_MANUAL},
				{"Brutefrc", TX_MODE_BRUTEFORCE},
				{"DeBruijn", TX_MODE_DEBRUIJN},
			},
		};

		OptionsField options_period_per_bit{
			{25 * 8, 0 * 8},
			5,
			PERIODS_PER_BIT,
		};

		NumberField field_repeat_min{
			{8 * 8, 4 * 8},
			5,
			{1, 100},
			1,
			' ',
		};

		NumberField field_pause_between_frames{
			{8 * 8, 6 * 8},
			5,
			{0, 100},
			1,
			' ',
		};

		SymField symfield_word{
			{1 * 8, 10 * 8},
			20,
			SymField::SYMFIELD_DEF,
		};

		Text text_format{
			{1 * 8, 12 * 8, 25 * 8, 16},
			"",
		};
	};

	///////////////////////////////////////////////////////////////////////////////
	// OOKTxFilesView

	class OOKTxFilesView : public View
	{
	public:
		std::function<void()> on_waveform_change_request{};
		std::function<void(std::string)> on_status_change{};

		OOKTxFilesView(NavigationView &nav, Rect parent_rect);
		void focus() override;
		void on_show() override;

		File::Size file_size{0};
		std::filesystem::path file_path{};

		// private:
		void on_file_changed(std::filesystem::path new_file_path);

		// UI related

		Labels labels{
			{{14 * 8, 0 * 8}, "Sh Pulse:", Color::light_grey()},
			{{28 * 8, 0 * 8}, "us", Color::light_grey()},
		};

		Text text_filename{
			{11 * 8, 0 * 16, 12 * 8, 16},
			"-"};

		Text text_duration{
			{11 * 8, 1 * 16, 6 * 8, 16},
			"-"};

		Button button_open{
			{0 * 8, 0 * 16, 10 * 8, 2 * 16},
			"Open file"};

		OptionsField options_period_per_bit{
			{23 * 8, 0 * 8},
			5,
			PERIODS_PER_BIT,
		};
	};

	///////////////////////////////////////////////////////////////////////////////
	// OOKTxDeBruijnView - used to debug some underneath logic

	class OOKTxDeBruijnView : public View
	{
	public:
		std::function<void()> on_waveform_change_request{};
		std::function<void(std::string)> on_status_change{};

		OOKTxDeBruijnView(NavigationView &nav, Rect parent_rect);
		void focus() override;
		void on_show() override;
		void on_hide() override;

		DeBruijnSequencer sequencer{8};

		void reset_symfield();

		// private:
		const std::string symfield_symbols = "01";

		// UI related
		Labels labels{
			{{1 * 8, 0 * 8}, "Word Length:", Color::light_grey()},
			{{1 * 8, 2 * 8}, "Fragments #:", Color::light_grey()},

			{{14 * 8, 0 * 8}, "Sh Pulse:", Color::light_grey()},
			{{28 * 8, 0 * 8}, "us", Color::light_grey()},
			// frame descriptors
			{{1 * 8, 6 * 8}, "Frag 0:", Color::light_grey()},
			{{1 * 8, 8 * 8}, "Frag 1:", Color::light_grey()},
		};

		NumberField field_wordlength{
			{14 * 8, 0 * 8},
			2,
			{1, 32},
			1,
			' ',
		};

		NumberField field_fragments{
			{14 * 8, 2 * 8},
			2,
			{1, 8},
			1,
			' ',
		};

		OptionsField options_period_per_bit{
			{23 * 8, 0 * 8},
			5,
			PERIODS_PER_BIT,
		};

		SymField symfield_fragment_off{
			{9 * 8, 6 * 8},
			16,
			SymField::SYMFIELD_DEF,
		};
		SymField symfield_fragment_on{
			{9 * 8, 8 * 8},
			16,
			SymField::SYMFIELD_DEF,
		};
	};

	///////////////////////////////////////////////////////////////////////////////
	// OOKTxView

	class OOKTxView : public View
	{
	public:
		OOKTxView(NavigationView &nav);
		~OOKTxView();
		void focus() override;
		void refresh();
		std::string title() const override { return "OOK TX"; };

	private:
		std::string err{""};

		// TX related
		std::unique_ptr<StreamReaderThread>
			stream_reader_thread{};
		void handle_stream_reader_thread_done(const uint32_t return_code);
		const size_t read_size{2048};
		const size_t buffer_count{3};

		void generate_frame();
		void progress_reset();
		void progress_update(uint64_t bytes);
		void draw_waveform();
		int16_t waveform_buffer[550];

		void start_tx();
		void on_tx_progress(const TXProgressMessage message);
		void tx(std::unique_ptr<stream::Reader> reader, uint32_t samples_per_bit, uint64_t max_bytes);
		void stop_tx();
		uint64_t tx_max_bytes{0};

		// general
		tx_modes tx_mode = TX_MODE_IDLE;
		cursor bruteforce_cursor{};

		std::vector<bool> frame_fragments{};
		std::vector<bool> on_symbol_fragments{};
		std::vector<bool> off_symbol_fragments{};

		// UI related
		NavigationView &nav_;
		Rect view_rect = {0, 4 * 8, 30 * 8, 17 * 8};

		OOKTxFilesView view_files{nav_, view_rect};
		OOKTxGeneratorView view_generator{nav_, view_rect};
		OOKTxDeBruijnView view_debruijn{nav_, view_rect};

		TabView tab_view{
			{"Files", Color::green(), &view_files},
			{"Encoders", Color::cyan(), &view_generator},
			{"DeBruijn", Color::green(), &view_debruijn},
		};

		Labels labels{
			{{1 * 8, 18 * 8}, "Waveform:", Color::light_grey()},
		};

		Checkbox checkbox_reversed{
			{20 * 8, 18 * 8},
			12,
			"Reverse",
			true,
		};

		Waveform waveform{
			{0, 21 * 8, 30 * 8, 32},
			waveform_buffer,
			0,
			0,
			true,
			Color::yellow(),
		};

		Text text_progress{
			{1 * 8, 13 * 16, 30 * 8, 16},
			"Ready",
		};

		ProgressBar progress_bar{
			{1 * 8, 13 * 16 + 20, 224, 16},
		};

		TransmitterView tx_view{
			16 * 16,
			50000,
			9,
		};

		///////////////////////////////////////////////////////////////////////
		// processor related handlers

		// handle thread completion and errors
		MessageHandlerRegistration message_handler_stream_reader_thread_error{
			Message::ID::StreamReaderThreadDone,
			[this](const Message *const p)
			{
				const auto message = *reinterpret_cast<const StreamReaderThreadDoneMessage *>(p);
				this->handle_stream_reader_thread_done(message.return_code);
			}};

		// handle tx progress
		MessageHandlerRegistration message_handler_tx_progress{
			Message::ID::TXProgress,
			[this](const Message *const p)
			{
				const auto message = *reinterpret_cast<const TXProgressMessage *>(p);
				this->on_tx_progress(message);
			},
		};
	};

} /* namespace ui */
