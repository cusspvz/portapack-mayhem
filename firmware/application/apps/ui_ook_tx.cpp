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

	OOKTxGeneratorView::OOKTxGeneratorView(NavigationView &, Rect parent_rect)
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
			&options_period_per_bit,
			&field_repeat_min,
			&field_pause_between_frames,
			&symfield_word,
			&text_format,
		});

		options_encoder.on_change = [this](size_t index, int32_t)
		{
			encoder_def = &encoder_defs[index];

			field_repeat_min.set_value(encoder_def->repeat_min);
			options_period_per_bit.set_by_value(encoder_def->shorter_pulse_period);
			field_pause_between_frames.set_value(encoder_def->pause_bits);

			reset_symfield();
			check_if_encoder_is_vuln_to_debruijn();

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

		options_encoder.set_options(enc_options);
	}

	void OOKTxGeneratorView::focus()
	{
		options_encoder.focus();
	}

	void OOKTxGeneratorView::on_show()
	{
		options_encoder.set_selected_index(0);

		if (on_waveform_change_request)
			on_waveform_change_request();
	}

	void OOKTxGeneratorView::check_if_encoder_is_vuln_to_debruijn()
	{
		// if the selected tx method is DEBRUIJN, check if the encoder is vulnerable to DEBRUIJN
		if (options_tx_method.selected_index_value() == TX_MODE_DEBRUIJN && encoder_def->sync_bit_length > 0)
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

	// std::string OOKTxGeneratorView::generate_frame(const uint32_t frame_part_index, const bool reversed)
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

		options_period_per_bit.set_by_value(32);
	}

	void OOKTxFilesView::focus()
	{
		// field_debug.focus();
	}

	void OOKTxFilesView::on_show()
	{
		if (on_waveform_change_request)
			on_waveform_change_request();
	}

	// uint32_t OOKTxFilesView::get_repeat_total() { return 1; }
	// uint32_t OOKTxFilesView::get_frame_part_total() { return 1; }

	// void OOKTxFilesView::reset_reader()
	// {
	// 	reader.get()->reset("0");
	// };

	// std::string OOKTxFilesView::generate_frame(const uint32_t frame_part_index, const bool reversed)
	// {
	// 	return "0";
	// }

	void OOKTxFilesView::on_file_changed(std::filesystem::path new_file_path)
	{

		File data_file;

		// Get file size
		auto data_open_error = data_file.open("/" + new_file_path.string());
		if (data_open_error.is_valid())
		{
			return;
		}

		file_path = new_file_path;
		file_size = data_file.size();

		text_filename.set(file_path.filename().string().substr(0, 12));
		text_duration.set(to_string_dec_uint(file_size * 8) + " bits");

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
			&options_period_per_bit,
			&symfield_fragment_off,
			&symfield_fragment_on,
		});

		field_wordlength.on_change = [this](uint32_t)
		{
			if (on_waveform_change_request)
				on_waveform_change_request();
		};

		field_fragments.on_change = [this](uint32_t)
		{
			reset_symfield();

			if (on_waveform_change_request)
				on_waveform_change_request();
		};

		symfield_fragment_off.on_change = [this]()
		{
			if (on_waveform_change_request)
				on_waveform_change_request();
		};
		symfield_fragment_on.on_change = [this]()
		{
			if (on_waveform_change_request)
				on_waveform_change_request();
		};

		// set default values
		field_wordlength.set_value(8);
		field_fragments.set_value(1);
		options_period_per_bit.set_by_value(256);

		// symfield_fragment_off.set_next_possibility();
		symfield_fragment_on.set_next_possibility();
	}

	void OOKTxDeBruijnView::focus()
	{
		field_wordlength.focus();
	}

	void OOKTxDeBruijnView::on_show()
	{
		if (on_waveform_change_request)
			on_waveform_change_request();
	}
	void OOKTxDeBruijnView::on_hide()
	{
		sequencer.thread_stop();
	}

	void OOKTxDeBruijnView::reset_symfield()
	{
		std::string format_string = "";
		uint32_t fragments_length = field_fragments.value();

		symfield_fragment_off.set_length(fragments_length);
		symfield_fragment_on.set_length(fragments_length);

		for (uint32_t i = 0; i < fragments_length; i++)
		{
			symfield_fragment_off.set_symbol_list(i, symfield_symbols);
			symfield_fragment_on.set_symbol_list(i, symfield_symbols);
		}
	}

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
			&waveform,
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
				progress_update(0);
			}
		};
		view_files.on_status_change = [this](const std::string e)
		{
			if (err != e)
			{
				err = e;
				progress_update(0);
			}
		};
		view_generator.on_status_change = [this](const std::string e)
		{
			if (err != e)
			{
				err = e;
				progress_update(0);
			}
		};

		view_generator.on_encoder_change = [this]()
		{
			// reset reversed checkbox
			checkbox_reversed.set_value(false);

			// // reset the debruijn sequencer in case the encoder is vulnerable
			// if (encoder_def->sync_bit_length == 0)
			// {
			// 	reset_debruijn();
			// }
		};

		// start on the generator tab
		tab_view.set_selected(1);
	}

	void OOKTxView::focus()
	{
		tab_view.focus();
	}

	void OOKTxView::refresh()
	{
		// check if we need to update the debruijn sequencer
		if (tab_view.selected() == 2)
		{
			auto target_wordlength = view_debruijn.field_wordlength.value();

			// only reinit the sequencer in case the wordlength differs
			if (view_debruijn.sequencer.n != target_wordlength)
			{
				view_debruijn.sequencer.init(target_wordlength);
				view_debruijn.sequencer.wait_for_buffer_completed();
			}
		}

		generate_frame();
		progress_update(0);
		draw_waveform();
	}

	OOKTxView::~OOKTxView()
	{
		if (stream_reader_thread)
			stream_reader_thread.reset();

		transmitter_model.disable();
		baseband::shutdown();
	}

	void OOKTxView::generate_frame()
	{
		// frame_fragments.clear();

		// if (tab_view.selected() == 0)
		// 	frame_fragments += view_files.generate_frame(
		// 		frame_parts_cursor.index,
		// 		checkbox_reversed.value());

		if (tab_view.selected() == 1)
		{
			uint8_t selected_symbol_indexes[view_generator.encoder_def->word_length]{};

			// extract values from symfield
			for (uint32_t i = 0; i < view_generator.encoder_def->word_length; i++)
				selected_symbol_indexes[i] = view_generator.symfield_word.get_sym(i);

			generate_frame_fragments(&frame_fragments, view_generator.encoder_def, selected_symbol_indexes, checkbox_reversed.value());
		}

		if (tab_view.selected() == 2)
		{
			frame_fragments.clear();

			// generate the frame fragments
			view_debruijn.symfield_fragment_on.value_bool_vector(&on_symbol_fragments);
			view_debruijn.symfield_fragment_off.value_bool_vector(&off_symbol_fragments);

			frame_fragments.reserve(view_debruijn.field_fragments.value() * view_debruijn.sequencer.waveform_cache.size());

			for (auto &&res : view_debruijn.sequencer.waveform_cache)
			{
				if (res)
					frame_fragments.insert(frame_fragments.end(), on_symbol_fragments.begin(), on_symbol_fragments.end());
				else
					frame_fragments.insert(frame_fragments.end(), off_symbol_fragments.begin(), off_symbol_fragments.end());
			}
		}
	}

	void
	OOKTxView::handle_stream_reader_thread_done(const uint32_t return_code)
	{
		// if (return_code == StreamReaderThread::END_OF_STREAM)
		if (return_code == StreamReaderThread::READ_ERROR)
			err = "Streaming error";

		stop_tx();
	}

	// NOTE: should be called after changing the tx_mode
	void OOKTxView::progress_reset()
	{
		progress_bar.set_max(100);
		progress_update(0);
	}

	void OOKTxView::progress_update(uint64_t bytes)
	{
		uint32_t progress = (uint32_t)((bytes / tx_max_bytes) * 100);
		progress_bar.set_max(100);
		progress_bar.set_value(progress);
		// progress_bar.set_max(tx_max_bytes);
		// progress_bar.set_value(bytes);

		if (err != "")
		{
			text_progress.set_style(&style_err);
			text_progress.set(err);
		}
		else if (tx_mode == TX_MODE_IDLE)
		{
			text_progress.set_style(&style_success);
			text_progress.set("Ready");
		}
		else
		{
			text_progress.set_style(&style_info);
			// text_progress.set("Transmitting (" + to_string_dec_uint(progress) + "%)");
			text_progress.set("Transmitting (" + to_string_dec_uint(bytes) + "/" + to_string_dec_uint(tx_max_bytes) + ")");
		}
	}

	void OOKTxView::start_tx()
	{
		uint32_t pulses_per_bit;

		if (tx_mode != TX_MODE_IDLE)
			stop_tx();

		// File Loader View TX
		if (tab_view.selected() == 0)
		{
			// TODO: disable access to all inputs

			pulses_per_bit = view_files.options_period_per_bit.selected_index_value();

			auto file_reader_p = std::make_unique<FileReader>();
			auto open_error = file_reader_p->open(view_files.file_path);

			if (open_error.is_valid())
			{
				text_progress.set("Error opening file.");
				return; // Fixes TX bug if there's a file error
			}

			tx_mode = TX_MODE_LOADER;

			// TODO: read the file size
			tx(std::move(file_reader_p), pulses_per_bit, view_files.file_size * 8);
		}

		// Generator View TX
		if (tab_view.selected() == 1)
		{
			// TODO: disable access to all inputs
			view_generator.symfield_word.set_focusable(false);

			// TODO: disable access to all inputs

			pulses_per_bit = view_generator.options_period_per_bit.selected_index_value();
			// TODO: Reader

			switch (view_generator.options_tx_method.selected_index_value())
			{
			case TX_MODE_MANUAL:
			case TX_MODE_BRUTEFORCE:
				uint64_t max_bytes = 0;
				auto ook_frame_reader_p = std::make_unique<OOKFrameReader>();

				ook_frame_reader_p->frame_fragments = &frame_fragments;
				ook_frame_reader_p->pauses_cursor.total = view_generator.field_pause_between_frames.value();
				ook_frame_reader_p->repetitions_cursor.total = view_generator.field_repeat_min.value();

				if (view_generator.options_tx_method.selected_index_value() == TX_MODE_MANUAL)
				{
					tx_mode = TX_MODE_MANUAL;

					// set max at the progress bar
					max_bytes = ook_frame_reader_p->length();
				}

				if (view_generator.options_tx_method.selected_index_value() == TX_MODE_BRUTEFORCE)
				{
					tx_mode = TX_MODE_BRUTEFORCE;
					bruteforce_cursor.reset();
					bruteforce_cursor.total = view_generator.symfield_word.get_possibilities_count();

					// set max at the progress bar
					ook_frame_reader_p->completition_requires_pause = true;
					max_bytes = ook_frame_reader_p->length() * bruteforce_cursor.total;

					ook_frame_reader_p->on_complete = [this](OOKFrameReader &reader)
					{
						if (this->bruteforce_cursor.is_done())
							return;

						this->bruteforce_cursor.bump();
						this->view_generator.symfield_word.set_next_possibility();
						generate_frame();

						reader.frame_fragments = &this->frame_fragments;
						reader.reset();

						// hide the waveform
						// waveform.hidden(true);
					};
				}

				ook_frame_reader_p->reset();
				tx(std::move(ook_frame_reader_p), pulses_per_bit, max_bytes);
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
			// TODO: disable access to all inputs

			pulses_per_bit = view_debruijn.options_period_per_bit.selected_index_value();
			tx_mode = TX_MODE_DEBRUIJN;

			// reader
			auto ook_debruijn_reader_p = std::make_unique<OOKDebruijnReader>();

			ook_debruijn_reader_p->sequencer = &view_debruijn.sequencer;

			// NOTE: no need to generate the symbols as we're rebuilding these at every refresh
			ook_debruijn_reader_p->on_symbol_fragments = &on_symbol_fragments;
			ook_debruijn_reader_p->off_symbol_fragments = &off_symbol_fragments;

			ook_debruijn_reader_p->reset();
			uint64_t max_bytes = ook_debruijn_reader_p->length();

			tx(std::move(ook_debruijn_reader_p), pulses_per_bit, max_bytes);
		}
	}

	void OOKTxView::tx(
		std::unique_ptr<stream::Reader> reader,
		uint32_t pulses_per_bit,
		uint64_t max_bytes)
	{
		if (!(bool)reader)
		{
			tx_mode = TX_MODE_IDLE;
			return;
		}

		tx_max_bytes = max_bytes;

		baseband::set_ook_data(pulses_per_bit);

		stream_reader_thread = std::make_unique<StreamReaderThread>(
			std::move(reader),
			read_size, buffer_count);

		tx_view.set_transmitting(true);
		transmitter_model.enable();
	}

	void OOKTxView::on_tx_progress(const TXProgressMessage message)
	{
		if (message.done)
			stop_tx();
		else
			progress_update(message.progress);
	}

	void OOKTxView::stop_tx()
	{
		if (stream_reader_thread)
			stream_reader_thread.reset();

		transmitter_model.disable();
		tx_mode = TX_MODE_IDLE;
		tx_view.set_transmitting(false);
		view_generator.symfield_word.set_focusable(true);

		// waveform.hidden(false);
		progress_reset();
	}

	void OOKTxView::draw_waveform()
	{
		uint16_t length = (uint16_t)std::min(frame_fragments.size(), sizeof(waveform_buffer));

		for (int16_t n = 0; n < length; n++)
			waveform_buffer[n] = frame_fragments.at(n) ? 1 : 0;

		waveform.set_length(length);
		waveform.set_dirty();
	}

} /* namespace ui */
