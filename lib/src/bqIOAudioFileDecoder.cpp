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

void IOAudioFileDecoder::set_clip_idx(Library *library, unsigned int clip_idx,
	const AudioClip &cur_clip)
{
	if (!_last_clip_idx_valid || clip_idx != _last_clip_idx) {
		if (_needs_reset_next_frame(library, cur_clip)) {
			_reset_next_send_frame();
		}

		_last_clip_idx = clip_idx;
		_last_clip_idx_valid = true;
	}
}

void IOAudioFileDecoder::set_song_id(Library *library, unsigned int song_id)
{
	if (!_last_song_id_valid || song_id != _last_song_id) {
		_open_file(library, song_id);
		_last_song_id = song_id;
		_last_song_id_valid = true;
	}
}

PlayheadChunk *IOAudioFileDecoder::decode(ma_uint64 from_frame)
{
	if (!_decoder_ready || _end_of_song) {
		return nullptr;
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

	_next_send_frame = actual_from_frame + _CHUNK_NUM_FRAMES;

	return chunk;
}

void IOAudioFileDecoder::invalidate_last_clip_idx()
{
	_last_clip_idx_valid = false;
}

void IOAudioFileDecoder::playhead_jumped()
{
	_reset_next_send_frame();
}

void IOAudioFileDecoder::_reset_next_send_frame()
{
	_next_send_frame = 0;
	_next_send_frame_valid = false;

	_end_of_song = false;
}

bool IOAudioFileDecoder::_needs_reset_next_frame(Library *library,
	const AudioClip &cur_clip)
{
	bool needs_reset_next_frame = true;

	if (library && _last_clip_valid) {
		if (cur_clip.song_id == _last_clip.song_id) {
			ma_uint64 frames_delta = llabs(static_cast<ma_int64>(
				cur_clip.first_frame) - static_cast<ma_int64>(
					_last_clip.first_frame));

			double beats_delta = fabs(cur_clip.start -
				_last_clip.start);

			ma_uint64 beats_delta_to_frames =
				library->beats_to_samples(cur_clip.song_id,
					beats_delta);

			ma_uint64 beats_frames_delta = llabs(
				static_cast<ma_int64>(beats_delta_to_frames) -
				static_cast<ma_int64>(frames_delta));

			// Accounts for floating-point (double) precision errors
			// An error of only two frames with the benefit of no
			// audio dropout is completely tolerable in most
			// situations.
			constexpr ma_uint64 BEATS_FRAMES_DELTA_TOLERANCE = 2;

			if (beats_frames_delta <=
				BEATS_FRAMES_DELTA_TOLERANCE) {
				needs_reset_next_frame = false;
			}
		}
	}

	_last_clip.start = cur_clip.start;
	_last_clip.first_frame = cur_clip.first_frame;
	_last_clip.song_id = cur_clip.song_id;
	_last_clip_valid = true;

	return needs_reset_next_frame;
}

void IOAudioFileDecoder::_open_file(Library *library, unsigned int song_id)
{
	_close_file();

	ma_decoder_config decoder_cfg = ma_decoder_config_init(ma_format_f32,
		_num_channels, _sample_rate);
	if (ma_decoder_init_file(library->filename(song_id).c_str(),
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
