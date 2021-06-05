#include "bqIOAudioFileDecoder.h"

namespace bq {
IOAudioFileDecoder::~IOAudioFileDecoder()
{
	_close_file();
}

void IOAudioFileDecoder::set_decode_config(ma_uint32 num_channels,
	ma_uint32 sample_rate)
{
	_num_channels = num_channels;
	_sample_rate = sample_rate;
}

void IOAudioFileDecoder::bind_library(Library *library)
{
	_library = library;
}

void IOAudioFileDecoder::set_clip_idx(unsigned int clip_idx)
{
	if (!_last_clip_idx_valid || clip_idx != _last_clip_idx) {
		reset_next_send_frame();

		_last_clip_idx = clip_idx;
		_last_clip_idx_valid = true;
	}
}

void IOAudioFileDecoder::set_song_id(unsigned int song_id)
{
	if (!_last_song_id_valid || song_id != _last_song_id) {
		_open_file(song_id);

		reset_next_send_frame();

		_last_song_id = song_id;
		_last_song_id_valid = true;
	}
}

PlayheadChunk *IOAudioFileDecoder::decode(ma_uint64 from_frame)
{
	if (!_decoder_ready || _end_of_song) {
		return nullptr;
	}

	if (from_frame < _last_from_frame) {
		reset_next_send_frame();
	}

	// If we skipped chunks (e.g. clip's sample offset was adjusted), reset
	// the next send frame to from_frame rather than decoding everything in
	// between (because those chunks won't be used)...
	if (from_frame > _next_send_frame + _CHUNK_NUM_FRAMES) {
		reset_next_send_frame();
	}

	ma_uint64 needs_chunk_threshold = _next_send_frame;
	// This if statement is necessary because these are *unsigned* ints - we
	// wouldn't want them to wrap around to a huge number if we subtracted
	// a larger number from a smaller number
	if (needs_chunk_threshold > _SEND_FRAME_WINDOW) {
		needs_chunk_threshold -= _SEND_FRAME_WINDOW;
	} else {
		needs_chunk_threshold = 0;
	}

	if (from_frame < needs_chunk_threshold) {
		return nullptr;
	}

	ma_uint64 actual_from_frame = _next_send_frame;
	if (!_next_send_frame_valid) {
		actual_from_frame = from_frame;
		_next_send_frame_valid = true;
	}

	if (_decoder_cur_frame != actual_from_frame) {
		if (ma_decoder_seek_to_pcm_frame(&_decoder, actual_from_frame)
			!= MA_SUCCESS) {
			_end_of_song = true;
			return nullptr;
		}
	}
	_decoder_cur_frame = actual_from_frame;

	PlayheadChunk *chunk = new PlayheadChunk;
	chunk->next = nullptr;
	chunk->song_id = _last_song_id;
	chunk->num_channels = _num_channels;
	chunk->sample_rate = _sample_rate;
	chunk->first_frame = actual_from_frame;
	chunk->num_frames = _CHUNK_NUM_FRAMES;
	chunk->frames = new float[_CHUNK_NUM_FRAMES * _num_channels];

	ma_uint64 num_decoded_frames = ma_decoder_read_pcm_frames(&_decoder,
		chunk->frames, _CHUNK_NUM_FRAMES);
	_decoder_cur_frame += num_decoded_frames;

	chunk->num_frames = num_decoded_frames;
	if (chunk->num_frames < _CHUNK_NUM_FRAMES) {
		_end_of_song = true;
	}
	if (chunk->num_frames <= 0) {
		delete[] chunk->frames;
		delete chunk;
		return nullptr;
	}

	_last_from_frame = from_frame;
	_next_send_frame = actual_from_frame + _CHUNK_NUM_FRAMES;

	return chunk;
}

void IOAudioFileDecoder::invalidate_last_clip_idx()
{
	_last_clip_idx_valid = false;
}

void IOAudioFileDecoder::reset_next_send_frame()
{
	_next_send_frame = 0;
	_next_send_frame_valid = false;

	_end_of_song = false;
}

void IOAudioFileDecoder::_open_file(unsigned int song_id)
{
	_close_file();

	ma_decoder_config decoder_cfg = ma_decoder_config_init(ma_format_f32,
		_num_channels, _sample_rate);
	if (ma_decoder_init_file(_library->filename(song_id).c_str(),
		&decoder_cfg, &_decoder) == MA_SUCCESS) {
		_decoder_ready = true;
	}
}

void IOAudioFileDecoder::_close_file()
{
	if (_decoder_ready) {
		ma_decoder_uninit(&_decoder);
	}

	_decoder_ready = false;
	_decoder_cur_frame = 0;
	_next_send_frame = 0;
	_next_send_frame_valid = false;

	_end_of_song = false;
}
}
