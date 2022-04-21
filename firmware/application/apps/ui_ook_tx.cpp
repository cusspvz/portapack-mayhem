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

#include "ui_ook_tx.hpp"
#include "baseband_api.hpp"
#include "string_format.hpp"
#include "ui_fileman.hpp"

using namespace portapack;

namespace ui
{

	///////////////////////////////////////////////////////////////////////////////
	// OOKTxGeneratorView

	OOKTxGeneratorView::OOKTxGeneratorView(NavigationView &nav, Rect parent_rect)
	{
		set_parent_rect(parent_rect);
		hidden(true);

		using option_t = std::pair<std::string, int32_t>;
		std::vector<option_t> enc_options;
		size_t i;

		// Default encoder def
		encoder_def = &encoder_defs[0];

		// Load encoder types in option field
		for (i = 0; i < ENC_TYPES_COUNT; i++)
			enc_options.emplace_back(std::make_pair(encoder_defs[i].name, i));

		add_children({
			&labels,
			&options_encoder,
			&options_tx_method,
			&field_clk,
			&field_frameduration,
			&options_period_per_symbol,
			&field_repeat_min,
			&field_pause_between_symbols,
			&symfield_word,
			&text_format,
		});

		options_encoder.on_change = [this](size_t index, int32_t)
		{
			encoder_def = &encoder_defs[index];

			field_clk.set_value(encoder_def->default_clk_speed / 1000);
			field_repeat_min.set_value(encoder_def->repeat_min);
			options_period_per_symbol.set_by_value(encoder_def->period_per_symbol);
			field_pause_between_symbols.set_value(encoder_def->pause_symbols);

			reset_symfield();
			check_if_encoder_is_vuln_to_debruijn();

			// reset the debruijn sequencer in case the encoder is vulnerable
			if (encoder_def->is_vuln_to_debruijn)
			{
				reset_debruijn();
			}

			if (on_encoder_change)
				on_encoder_change();

			if (on_waveform_change_request)
				on_waveform_change_request();
		};

		options_tx_method.on_change = [this](size_t, int32_t value)
		{
			if (value == TX_MODE_DEBRUIJN)
			{
				// Set the repeat to 0
				field_repeat_min.set_value(0);
			}
			else
			{
				// set to the default repeat min at the selected encoder
				field_repeat_min.set_value(encoder_def->repeat_min);
			}

			reset_symfield();
			check_if_encoder_is_vuln_to_debruijn();

			if (on_waveform_change_request)
				on_waveform_change_request();
		};

		symfield_word.on_change = [this]()
		{
			if (on_waveform_change_request)
				on_waveform_change_request();
		};

		// Selecting input clock changes symbol and word duration
		field_clk.on_change = [this](int32_t value)
		{
			// value is in kHz, new_value is in us
			int32_t new_value = (options_period_per_symbol.selected_index_value() * 1000000) / (value * 1000);
			if (new_value != field_frameduration.value())
				field_frameduration.set_value(new_value * encoder_def->word_length, false);
		};

		options_period_per_symbol.on_change = [this](size_t, int32_t)
		{
			// trigger the change on both fields
			field_clk.on_change(field_clk.value());
		};

		options_encoder.set_options(enc_options);
	}

	void OOKTxGeneratorView::focus()
	{
		options_encoder.focus();
	}

	void OOKTxGeneratorView::on_show()
	{
		options_encoder.set_selected_index(0);
	}

	void OOKTxGeneratorView::check_if_encoder_is_vuln_to_debruijn()
	{
		// if the selected tx method is DEBRUIJN, check if the encoder is vulnerable to DEBRUIJN
		if (options_tx_method.selected_index_value() == TX_MODE_DEBRUIJN && !encoder_def->is_vuln_to_debruijn)
		{
			if (on_status_change)
				on_status_change("Not vuln to DeBruijn");
			return;
		}

		if (on_status_change)
			on_status_change("");
	}

	void OOKTxGeneratorView::reset_symfield()
	{
		char symbol_type;
		std::string format_string = "";
		size_t word_length = encoder_def->word_length;

		symfield_word.set_length(word_length);

		size_t n = 0, i = 0;
		while (n < word_length)
		{
			symbol_type = encoder_def->word_format[i++];
			if (symbol_type == 'A')
			{
				symfield_word.set_symbol_list(n++, encoder_def->symfield_address_symbols);
				format_string += 'A';
			}
			else if (symbol_type == 'D')
			{
				symfield_word.set_symbol_list(n++, encoder_def->symfield_data_symbols);
				format_string += 'D';
			}
		}

		// Ugly :( Pad to erase
		format_string.append(24 - format_string.size(), ' ');

		text_format.set(format_string);
	}

	void OOKTxGeneratorView::reset_debruijn()
	{
		if (
			debruijn_sequencer.k == 2 &&
			debruijn_sequencer.n == encoder_def->word_length &&
			debruijn_sequencer.sequence.length() > 0)
			return;

		debruijn_sequencer.init("01", encoder_def->word_length, 32);

		// TODO: we need to stream the debruijn sequence instead of generating it all at once!
		// disabled in the encoder view as it just screws up on 16bits
		// debruijn_sequencer.generate();
	}

	// const char *OOKTxGeneratorView::get_symbols_bit_fragments(const uint8_t index, const bool reversed)
	// {
	// 	if (reversed)
	// 	{
	// 		return encoder_def->symbols_bit_fragments[(index == 0) ? 1 : 0];
	// 	}

	// 	return encoder_def->symbols_bit_fragments[index];
	// }

	// uint32_t OOKTxGeneratorView::get_repeat_total()
	// {
	// 	switch (options_tx_method.selected_index_value())
	// 	{
	// 	case TX_MODE_MANUAL:
	// 	case TX_MODE_BRUTEFORCE:
	// 		return field_repeat_min.value();

	// 	case TX_MODE_DEBRUIJN:
	// 		return 1;
	// 	}

	// 	return 1;
	// }

	// uint32_t OOKTxGeneratorView::get_frame_part_total()
	// {
	// 	switch (options_tx_method.selected_index_value())
	// 	{
	// 	case TX_MODE_BRUTEFORCE:
	// 		return symfield_word.get_possibilities_count();

	// 	case TX_MODE_DEBRUIJN: // check the size of the whole debruijn sequence
	// 		return debruijn_sequencer.get_total_parts();

	// 	case TX_MODE_MANUAL:
	// 		return 1;
	// 	}

	// 	return 1;
	// }

	// void OOKTxGeneratorView::reset_reader()
	// {
	// 	uint8_t selected_symbol_indexes[encoder_def->word_length]{};

	// 	// extract values from symfield
	// 	for (uint32_t i = 0; i < encoder_def->word_length; i++)
	// 		selected_symbol_indexes[i] = symfield_word.get_sym(i);

	// 	// apply to the current reader
	// 	reader.get()->reset(encoder_def, selected_symbol_indexes, false);
	// };

	// std::string OOKTxGeneratorView::generate_frame_part(const uint32_t frame_part_index, const bool reversed)
	// {
	// 	int32_t mode = options_tx_method.selected_index_value();

	// 	// NOTE: bruteforce might need to be check to enforce the frame_part_index
	// 	// TODO: :this:

	// 	if (mode == TX_MODE_MANUAL || mode == TX_MODE_BRUTEFORCE)
	// 	{
	// 		std::string frame_fragments = "";

	// 		for (uint8_t i = 0; i < encoder_def->word_length; i++)
	// 			if (encoder_def->word_format[i] == 'S')
	// 				frame_fragments += encoder_def->sync_bit_fragment;
	// 			else
	// 				frame_fragments += get_symbols_bit_fragments(symfield_word.get_sym(i), reversed);

	// 		return frame_fragments;
	// 	}

	// 	if (mode == TX_MODE_DEBRUIJN)
	// 	{
	// 		if (!encoder_def->is_vuln_to_debruijn)
	// 			return "0";

	// 		std::string frame_fragments = "";
	// 		std::string frame_part = debruijn_sequencer.get_part(frame_part_index);

	// 		for (uint8_t i = 0; i < frame_part.length(); i++)
	// 		{
	// 			frame_fragments += get_symbols_bit_fragments(("1" == &frame_part[i]) ? 1 : 0, reversed);
	// 		}

	// 		return frame_fragments;
	// 	}

	// 	return "0";
	// }

	// 	///////////////////////////////////////////////////////////////////////////////
	// 	// OOKTxFilesView

	OOKTxFilesView::OOKTxFilesView(
		NavigationView &nav, Rect parent_rect)
	{
		set_parent_rect(parent_rect);
		hidden(true);

		add_children({
			&labels,
			&options_period_per_bit,
			&button_open,
		});

		button_open.on_select = [this, &nav](Button &)
		{
			auto open_view = nav.push<FileLoadView>(".ASK");
			open_view->on_changed = [this](std::filesystem::path new_file_path)
			{
				on_file_changed(new_file_path);
			};
		};
	}

	void OOKTxFilesView::focus()
	{
		// field_debug.focus();
	}

	// uint32_t OOKTxFilesView::get_repeat_total() { return 1; }
	// uint32_t OOKTxFilesView::get_frame_part_total() { return 1; }

	// void OOKTxFilesView::reset_reader()
	// {
	// 	reader.get()->reset("0");
	// };

	// std::string OOKTxFilesView::generate_frame_part(const uint32_t frame_part_index, const bool reversed)
	// {
	// 	return "0";
	// }

	void OOKTxFilesView::on_file_changed(std::filesystem::path new_file_path)
	{

		File data_file, info_file;
		// char file_data[257];

		// Get file size
		auto data_open_error = data_file.open("/" + new_file_path.string());
		if (data_open_error.is_valid())
		{
			return;
		}

		file_path = new_file_path;

		// Get original record frequency if available
		// std::filesystem::path info_file_path = file_path;
		// info_file_path.replace_extension(u".TXT");

		// sample_rate = 500000;

		// auto info_open_error = info_file.open("/" + info_file_path.string());
		// if (!info_open_error.is_valid())
		// {
		// 	memset(file_data, 0, 257);
		// 	auto read_size = info_file.read(file_data, 256);
		// 	if (!read_size.is_error())
		// 	{
		// 		auto pos1 = strstr(file_data, "samples_per_bit=");
		// 		if (pos1)
		// 		{
		// 			pos1 += 16;
		// 			field_frequency.set_value(strtoll(pos1, nullptr, 10));
		// 		}
		// 	}
		// }

		// text_sample_rate.set(unit_auto_scale(sample_rate, 3, 0) + "Hz");

		// auto file_size = data_file.size();
		// auto duration = (file_size * 1000) / (2 * 2 * sample_rate);

		// progressbar.set_max(file_size);
		// text_filename.set(file_path.filename().string().substr(0, 12));
		// text_duration.set(to_string_time_ms(duration));

		// button_play.focus();
	}

	///////////////////////////////////////////////////////////////////////////////
	// OOKTxDeBruijnView

	OOKTxDeBruijnView::OOKTxDeBruijnView(
		NavigationView &, Rect parent_rect)
	{
		set_parent_rect(parent_rect);
		hidden(true);

		add_children({
			&labels,
			&field_wordlength,
			&field_fragments,
			&field_clk,
			&field_frameduration,
			&options_period_per_symbol,
			&field_pause_between_symbols,
			&symfield_fragment_0,
			&symfield_fragment_1,
		});

		field_wordlength.on_change = [this](uint32_t)
		{
			reset_debruijn();

			if (on_waveform_change_request)
				on_waveform_change_request();
		};

		field_fragments.on_change = [this](uint32_t)
		{
			reset_symfield();

			if (on_waveform_change_request)
				on_waveform_change_request();
		};

		symfield_fragment_0.on_change = [this]()
		{
			if (on_waveform_change_request)
				on_waveform_change_request();
		};
		symfield_fragment_1.on_change = [this]()
		{
			if (on_waveform_change_request)
				on_waveform_change_request();
		};

		// Selecting input clock changes symbol and word duration
		field_clk.on_change = [this](int32_t value)
		{
			// value is in kHz, new_value is in us
			int32_t new_value = (options_period_per_symbol.selected_index_value() * 1000000) / (value * 1000);
			if (new_value != field_frameduration.value())
				field_frameduration.set_value(new_value * field_wordlength.value(), false);
		};

		// Selecting word duration changes input clock and symbol duration
		field_frameduration.on_change = [this](int32_t value)
		{
			// value is in us, new_value is in kHz
			int32_t new_value = (value * 1000) / (field_wordlength.value() * options_period_per_symbol.selected_index_value());
			if (new_value != field_clk.value())
				field_clk.set_value(1000000 / new_value, false);
		};

		options_period_per_symbol.on_change = [this](size_t, int32_t)
		{
			// trigger the change on both fields
			field_clk.on_change(field_clk.value());
			field_frameduration.on_change(field_frameduration.value());
		};

		// set default values
		field_wordlength.set_value(4);
		field_fragments.set_value(4);
		field_clk.set_value(250);
		options_period_per_symbol.set_by_value(10);
		symfield_fragment_0.set_next_possibility();

		for (uint32_t i = 0; i < field_fragments.value(); i++)
			symfield_fragment_1.set_sym(i, 1);
	}

	void OOKTxDeBruijnView::focus()
	{
		field_wordlength.focus();
	}

	void OOKTxDeBruijnView::reset_symfield()
	{
		std::string format_string = "";
		uint32_t fragments_length = field_fragments.value();

		symfield_fragment_0.set_length(fragments_length);
		symfield_fragment_1.set_length(fragments_length);

		for (uint32_t i = 0; i < fragments_length; i++)
		{
			symfield_fragment_0.set_symbol_list(i, symfield_symbols);
			symfield_fragment_1.set_symbol_list(i, symfield_symbols);
		}
	}

	void OOKTxDeBruijnView::reset_debruijn()
	{
		uint32_t word_length = field_wordlength.value();

		if (
			debruijn_sequencer.k == 2 &&
			debruijn_sequencer.n == word_length &&
			debruijn_sequencer.sequence.length() > 0)
			return;

		debruijn_sequencer.init(symfield_symbols, word_length, 32);

		// TODO: we need to stream the debruijn sequence instead of generating it all at once!
		debruijn_sequencer.generate();
	}

	// uint32_t OOKTxDeBruijnView::get_repeat_total() { return 1; }
	// uint32_t OOKTxDeBruijnView::get_frame_part_total()
	// {
	// 	return debruijn_sequencer.get_total_parts();
	// }

	// void OOKTxDeBruijnView::reset_reader()
	// {
	// 	reader.get()->reset("0");
	// };

	// std::string OOKTxDeBruijnView::generate_frame_part(const uint32_t frame_part_index, const bool reverse)
	// // TODO: we still need to implement the reverse flag
	// {
	// 	uint32_t fragments_length = field_fragments.value();
	// 	std::string fragment_0 = ""; // symfield_fragment_0.value_string();
	// 	std::string fragment_1 = ""; // symfield_fragment_1.value_string();

	// 	// build fragments
	// 	for (uint32_t i = 0; i < fragments_length; i++)
	// 	{
	// 		fragment_0 += symfield_symbols[symfield_fragment_0.get_sym(i)];
	// 		fragment_1 += symfield_symbols[symfield_fragment_1.get_sym(i)];
	// 	}

	// 	std::string frame_fragments = "";
	// 	std::string frame_part = debruijn_sequencer.get_part(frame_part_index);

	// 	for (uint32_t i = 0; i < frame_part.length(); i++)
	// 	{
	// 		bool bit = frame_part[i] == symfield_symbols[1];

	// 		if (reverse)
	// 			bit = !bit;

	// 		frame_fragments += (bit) ? fragment_1 : fragment_0;
	// 	}

	// 	return frame_fragments;
	// }

	// 	///////////////////////////////////////////////////////////////////////////////
	// 	// OOKTxView

	OOKTxView::OOKTxView(NavigationView &nav) : nav_{nav}
	{
		baseband::run_image(portapack::spi_flash::image_tag_ook);

		transmitter_model.set_sampling_rate(OOK_SAMPLERATE);
		transmitter_model.set_baseband_bandwidth(1750000);

		add_children({
			&labels,
			&tab_view,

			// tab views
			&view_debruijn,
			&view_files,
			&view_generator,

			&checkbox_reversed,
			// &waveform,
			&text_progress,
			&progress_bar,
			&tx_view,
		});

		// Event listeners
		tx_view.on_edit_frequency = [this, &nav]()
		{
			auto new_view = nav.push<FrequencyKeypadView>(transmitter_model.tuning_frequency());
			new_view->on_changed = [this](rf::Frequency f)
			{
				transmitter_model.set_tuning_frequency(f);
			};
		};

		tx_view.on_start = [this]()
		{
			start_tx();
		};

		tx_view.on_stop = [this]()
		{
			stop_tx();
		};

		// whenever the checkbox changes, rerender the waveform
		checkbox_reversed.on_select = [this](Checkbox &, bool)
		{
			refresh();
		};

		// View hooks

		view_files.on_waveform_change_request = [this]()
		{
			refresh();
		};
		view_generator.on_waveform_change_request = [this]()
		{
			refresh();
		};
		view_debruijn.on_waveform_change_request = [this]()
		{
			refresh();
		};

		view_debruijn.on_status_change = [this](const std::string e)
		{
			if (err != e)
			{
				err = e;
				progress_update();
			}
		};
		view_files.on_status_change = [this](const std::string e)
		{
			if (err != e)
			{
				err = e;
				progress_update();
			}
		};
		view_generator.on_status_change = [this](const std::string e)
		{
			if (err != e)
			{
				err = e;
				progress_update();
			}
		};

		view_generator.on_encoder_change = [this]()
		{
			// reset reversed checkbox
			checkbox_reversed.set_value(false);
		};

		// start on debruijn generator
		tab_view.set_selected(0);
		// tab_view.set_selected(2);
	}

	void OOKTxView::focus()
	{
		tab_view.focus();
		refresh();
	}

	void OOKTxView::refresh()
	{
		reset_cursors();
		// generate_frame_part();
		progress_update();
		// draw_waveform();
	}

	OOKTxView::~OOKTxView()
	{
		transmitter_model.disable();
		baseband::shutdown();
	}

	// TX methods
	// void OOKTxView::generate_frame_part()
	// {
	// 	frame_fragments.clear();

	// 	if (tab_view.selected() == 0)
	// 		frame_fragments += view_files.generate_frame_part(
	// 			frame_parts_cursor.index,
	// 			checkbox_reversed.value());

	// 	if (tab_view.selected() == 1)
	// 		frame_fragments += view_generator.generate_frame_part(
	// 			frame_parts_cursor.index,
	// 			checkbox_reversed.value());

	// 	if (tab_view.selected() == 2)
	// 		frame_fragments += view_debruijn.generate_frame_part(
	// 			frame_parts_cursor.index,
	// 			checkbox_reversed.value());
	// }

	void OOKTxView::set_ready()
	{
		ready_signal = true;
	}

	void
	OOKTxView::handle_stream_reader_thread_done(const uint32_t return_code)
	{
		// if (return_code == StreamReaderThread::END_OF_STREAM)
		if (return_code == StreamReaderThread::READ_ERROR)
			err = "Streaming error";

		stop_tx();
		progress_reset();
	}

	// NOTE: should be called after changing the tx_mode
	void OOKTxView::progress_reset()
	{
		progress_bar.set_max(frame_parts_cursor.total * repeat_cursor.total);
		progress_update();
	}

	void OOKTxView::progress_update()
	{
		if (err != "")
		{
			text_progress.set_style(&style_err);
			text_progress.set(err);
		}
		else if (tx_mode == TX_MODE_IDLE)
		{
			text_progress.set_style(&style_success);
			text_progress.set("Ready F(" + to_string_dec_uint(frame_parts_cursor.total) + ")" + "R(" + to_string_dec_uint(repeat_cursor.total) + ")");
			progress_bar.set_value(0);
		}
		else
		{
			text_progress.set_style(&style_info);
			text_progress.set(
				to_string_dec_uint(frame_parts_cursor.index + 1) + "/" + to_string_dec_uint(frame_parts_cursor.total) +
				" (" + to_string_dec_uint(repeat_cursor.index + 1) + "/" + to_string_dec_uint(repeat_cursor.total) + ")");
		}

		progress_bar.set_value(frame_parts_cursor.index * repeat_cursor.total + repeat_cursor.index);
	}

	void OOKTxView::start_tx()
	{
		uint32_t samples_per_bit;

		if (tx_mode != TX_MODE_IDLE)
			stop_tx();

		// File Loader View TX
		if (tab_view.selected() == 0)
		{
			// TODO: disable access to all inputs

			samples_per_bit = OOK_SAMPLERATE / (1000000 / (view_files.options_period_per_bit.selected_index_value()));

			// auto open_error = p->open(view_files.file_path);

			// if (open_error.is_valid())
			// {
			// 	text_progress.set("Error opening file.");
			// 	// return; // Fixes TX bug if there's a file error
			// }
			auto file_reader_p = std::make_unique<FileReader>();
			// auto p = std::make_unique<OOKEncoderReader>();

			tx(std::move(file_reader_p), samples_per_bit);
		}

		// Generator View TX
		if (tab_view.selected() == 1)
		{
			// TODO: disable access to all inputs
			view_generator.symfield_word.set_focusable(false);

			// TODO: disable access to all inputs

			samples_per_bit = OOK_SAMPLERATE / ((view_generator.field_clk.value() * 1000) / (view_generator.options_period_per_symbol.selected_index_value() / view_generator.encoder_def->bit_fragments_length_per_symbol));
			// TODO: Reader

			switch (view_generator.options_tx_method.selected_index_value())
			{
			case TX_MODE_MANUAL:
			case TX_MODE_BRUTEFORCE:
				uint8_t selected_symbol_indexes[view_generator.encoder_def->word_length]{};

				// extract values from symfield
				for (uint32_t i = 0; i < view_generator.encoder_def->word_length; i++)
					selected_symbol_indexes[i] = view_generator.symfield_word.get_sym(i);

				auto ook_encoder_reader_p = std::make_unique<OOKEncoderReader>();

				ook_encoder_reader_p->frame_fragments = generate_frame_fragments(view_generator.encoder_def, selected_symbol_indexes, checkbox_reversed.value());
				ook_encoder_reader_p->pauses_cursor.total = view_generator.field_pause_between_symbols.value();
				ook_encoder_reader_p->repetitions_cursor.total = view_generator.field_repeat_min.value();

				if (view_generator.options_tx_method.selected_index_value() == TX_MODE_MANUAL)
				{
					tx_mode = TX_MODE_MANUAL;
				}

				if (view_generator.options_tx_method.selected_index_value() == TX_MODE_BRUTEFORCE)
				{
					tx_mode = TX_MODE_BRUTEFORCE;
					bruteforce_cursor.reset();
					bruteforce_cursor.total = view_generator.symfield_word.get_possibilities_count();

					ook_encoder_reader_p->on_complete = [this](OOKEncoderReader &reader)
					{
						if (this->bruteforce_cursor.is_done())
							return;

						uint8_t selected_symbol_indexes[view_generator.encoder_def->word_length]{};

						this->view_generator.symfield_word.set_next_possibility();

						// extract values from symfield
						for (uint32_t i = 0; i < this->view_generator.encoder_def->word_length; i++)
							selected_symbol_indexes[i] = this->view_generator.symfield_word.get_sym(i);

						reader.frame_fragments = generate_frame_fragments(
							this->view_generator.encoder_def,
							selected_symbol_indexes,
							checkbox_reversed.value());

						this->bruteforce_cursor.bump();
						reader.reset();
					};
				}

				ook_encoder_reader_p->reset();
				tx(std::move(ook_encoder_reader_p), samples_per_bit);
				break;

				// case TX_MODE_DEBRUIJN:
				// 	if (!view_generator.encoder_def->is_vuln_to_debruijn)
				// 	{
				// 		stop_tx();
				// 		view_generator.check_if_encoder_is_vuln_to_debruijn();
				// 		return;
				// 	}
			}
		}

		// DeBruijn View TX
		if (tab_view.selected() == 2)
		{
			tx_mode = TX_MODE_DEBRUIJN;
			view_debruijn.reset_debruijn();

			// TODO: disable access to all inputs

			samples_per_bit = OOK_SAMPLERATE / ((view_debruijn.field_clk.value() * 1000) / (view_debruijn.options_period_per_symbol.selected_index_value() / view_debruijn.field_fragments.value()));
			// TODO: reader
		}
	}

	void OOKTxView::tx(
		std::unique_ptr<stream::Reader> reader,
		uint32_t samples_per_bit)
	{
		size_t read_size = 16;
		size_t buffer_count = 6;

		if (!(bool)reader)
		{
			tx_mode = TX_MODE_IDLE;
			return;
		}

		reset_cursors();
		// progress_reset();
		tx_view.set_transmitting(true);

		baseband::set_ook_data(samples_per_bit);
		stream_reader_thread = std::make_unique<StreamReaderThread>(
			std::move(reader),
			read_size, buffer_count,
			&ready_signal,
			[this](uint32_t return_code)
			{
				this->text_progress.set("return_code " + to_string_dec_uint(return_code));

				StreamReaderThreadDoneMessage message{return_code};
				EventDispatcher::send_message(message);
			});

		transmitter_model.enable();
	}

	// on_tx_progress
	// this method captures a tx message and decides what to do next.
	// ideally, it should attempt first to repeat the frame as repetitions are
	// handled by the baseband logic and then it should attempt to send the other
	// frame parts.
	void OOKTxView::on_tx_progress(const TXProgressMessage message)
	{

		if (message.done)
		{
			text_progress.set("done");
			stop_tx();
		}
		else
		{
			text_progress.set("on_tx_progress " + to_string_dec_uint(message.progress));
		}

		// if (tx_mode == TX_MODE_IDLE)
		// 	return;

		// // Update cursors
		// repeat_cursor.index++;
		// if (done)
		// {
		// 	frame_parts_cursor.index++;
		// }

		// // check if we have other frames to transmit or if we want to stop transmission
		// if (done)
		// {

		// 	if (repeat_cursor.is_done())
		// 	{
		// 		if (frame_parts_cursor.is_done())
		// 		{
		// 			stop_tx();
		// 			return;
		// 		}

		// 		// this needs to be assessed here, if we reset it before this may cause
		// 		// the last frame to be skipped (assuming, didn't test it)
		// 		repeat_cursor.index = 0;
		// 	}

		// 	if (view_generator.options_tx_method.selected_index_value() == TX_MODE_BRUTEFORCE)
		// 	{
		// 		view_generator.symfield_word.set_next_possibility();
		// 	}

		// 	// call tx
		// 	tx();
		// }
		// else
		// {
		// 	// in case we're not done yet, we still want to update the progress
		// 	progress_update();
		// }
	}

	void OOKTxView::stop_tx()
	{
		// TODO: this should be uncommented
		if (stream_reader_thread)
			stream_reader_thread.reset();

		transmitter_model.disable();
		tx_mode = TX_MODE_IDLE;
		tx_view.set_transmitting(false);
		view_generator.symfield_word.set_focusable(true);
		ready_signal = false;

		reset_cursors();
		progress_reset();
		// generate_frame_part();
		// draw_waveform();
	}

	void OOKTxView::reset_cursors()
	{
		repeat_cursor.reset();
		frame_parts_cursor.reset();

		// get the correct repeat min
		repeat_cursor.total = view_generator.field_repeat_min.value();

		// Generator
		switch (tab_view.selected())
		{
		// File Loader View TX
		case 0:
			// repeat_cursor.total = view_files.get_repeat_total();
			// frame_parts_cursor.total = view_files.get_frame_part_total();
			break;

		// Generator View TX
		case 1:
			// repeat_cursor.total = view_generator.get_repeat_total();
			// frame_parts_cursor.total = view_generator.get_frame_part_total();
			break;

		// DeBruijn TX
		case 2:
			// repeat_cursor.total = view_debruijn.get_repeat_total();
			// frame_parts_cursor.total = view_debruijn.get_frame_part_total();
			break;
		}
	}

	// void OOKTxView::draw_waveform()
	// {
	// 	size_t length = frame_fragments.length();

	// 	for (int16_t n = 0; n < length; n++)
	// 		waveform_buffer[n] = (frame_fragments[n] == '0') ? 0 : 1;

	// 	waveform.set_length(length);
	// 	waveform.set_dirty();
	// }

} /* namespace ui */
