#include "bqAudioPlayhead.h"
#include "bqIOEngine.h"

namespace bq {
AudioPlayhead::~AudioPlayhead()
{
	for (unsigned int i = 0; i < _NUM_TRACKS; ++i) {
		_pop_all_chunks(i);

		soundtouch_destroyInstance(_st[i]);
		_st[i] = nullptr;
	}

	delete[] _st_src;
}

void AudioPlayhead::set_playback_config(ma_uint32 num_channels,
	ma_uint32 sample_rate)
{
	_num_channels = num_channels;
	_sample_rate = sample_rate;

	if (_st_src) {
		delete[] _st_src;
	}

	_st_src = new float[_NUM_ST_SRC_FRAMES * num_channels];

	for (unsigned int i = 0; i < _NUM_TRACKS; ++i) {
		_setup_soundtouch(_st[i]);
	}
}

void AudioPlayhead::bind_io_engine(IOEngine *io)
{
	_io = io;
}

void AudioPlayhead::bind_library(Library *library)
{
	_library = library;
}

ma_uint64 AudioPlayhead::pull_stretch(double master_bpm, unsigned int track_idx,
	unsigned int clip_idx, AudioClip &clip, double song_bpm, float *dest,
	ma_uint64 first_frame, ma_uint64 num_frames)
{
	if (!_is_track_valid(track_idx)) {
		return 0;
	}

	set_cur_clip_idx(track_idx, clip_idx, clip);
	set_cur_song_id(track_idx, clip.song_id);

	HANDLE st = _st[track_idx];

	if (_needs_seek[track_idx]) {
		soundtouch_clear(st);
		_cache[track_idx].cur_want_frame = first_frame;
		_needs_seek[track_idx] = false;
	}

	// TODO: Does updating these values have any performance impact? Should
	// they only be set on the SoundTouch instance if they have changed?
	soundtouch_setPitchSemiTones(st, static_cast<float>(clip.pitch_shift));
	soundtouch_setTempo(st, static_cast<float>(master_bpm / song_bpm));

	ma_uint64 total_num_received = 0;
	bool pull_failed = false;

	while (total_num_received < num_frames) {
		if (soundtouch_numSamples(st) == 0) {
			ma_uint64 num_want = _NUM_ST_SRC_FRAMES; // for clarity
			ma_uint64 num_pulled = _pull(track_idx, clip, _st_src,
				num_want);
			if (num_pulled > 0) {
				soundtouch_putSamples(st, _st_src,
					static_cast<unsigned int>(num_pulled));
			}
			if (num_pulled < num_want) {
				pull_failed = true;
			}
		}

		unsigned int max_num_receive = static_cast<unsigned int>(
			num_frames - total_num_received);
		float *cur_dest = dest + (total_num_received * _num_channels);
		int cur_num_received = soundtouch_receiveSamples(st, cur_dest,
			max_num_receive);
		total_num_received += cur_num_received;

		if (cur_num_received == 0 && pull_failed) {
			break;
		}
	}

	return total_num_received;
}

void AudioPlayhead::receive_chunk(unsigned int track, PlayheadChunk *chunk)
{
	if (_is_track_valid(track)) {
		_ChunksList &chunks = _cache[track];
		if (chunks.tail) {
			chunks.tail->next = chunk;
			chunks.tail = chunk;
		} else {
			chunks.head = chunk;
			chunks.tail = chunk;
		}
	} else {
		_delete_chunk(chunk);
	}
}

double AudioPlayhead::get_beat()
{
	return _beat;
}

void AudioPlayhead::jump(double beat)
{
	if (beat != _beat) {
		for (unsigned int i = 0; i < _NUM_TRACKS; ++i) {
			_pop_all_chunks(i);
			_needs_seek[i] = true;
		}
		_beat = beat;
	}
}

void AudioPlayhead::invalidate_cur_clip_idx(unsigned int track_idx)
{
	if (_is_track_valid(track_idx)) {
		_cur_clip_idxs_valid[track_idx] = false;
	}
}

void AudioPlayhead::advance(double beats)
{
	_beat = _beat + beats;
}

unsigned int AudioPlayhead::get_cur_clip_idx(unsigned int track_idx)
{
	unsigned int cur_clip_idx = 0;

	if (_is_track_valid(track_idx)) {
		cur_clip_idx = _cur_clip_idxs[track_idx];
	}

	return cur_clip_idx;
}

unsigned int AudioPlayhead::get_cur_song_id(unsigned int track_idx)
{
	unsigned int cur_song_id = 0;

	if (_is_track_valid(track_idx)) {
		cur_song_id = _cur_song_ids[track_idx];
	}

	return cur_song_id;
}

void AudioPlayhead::set_cur_clip_idx(unsigned int track_idx,
	unsigned int cur_clip_idx, AudioClip &cur_clip)
{
	if (_is_track_valid(track_idx)) {
		if (cur_clip_idx != _cur_clip_idxs[track_idx] ||
			!_cur_clip_idxs_valid[track_idx]) {
			if (_needs_pop_and_seek(track_idx, cur_clip)) {
				_pop_all_chunks(track_idx);
				_needs_seek[track_idx] = true;
			}

			_cur_clip_idxs[track_idx] = cur_clip_idx;
			_cur_clip_idxs_valid[track_idx] = true;
		}
	}
}

void AudioPlayhead::set_cur_song_id(unsigned int track_idx,
	unsigned int cur_song_id)
{
	if (_is_track_valid(track_idx)) {
		if (cur_song_id != _cur_song_ids[track_idx] ||
			!_cur_song_ids_valid[track_idx]) {
			_pop_all_chunks(track_idx);
			_needs_seek[track_idx] = true;
			_cur_song_ids[track_idx] = cur_song_id;
			_cur_song_ids_valid[track_idx] = true;
		}
	}
}

void AudioPlayhead::_setup_soundtouch(HANDLE &st)
{
	if (st) {
		soundtouch_clear(st);
	} else {
		st = soundtouch_createInstance();
	}

	soundtouch_setChannels(st, _num_channels);
	soundtouch_setSampleRate(st, _sample_rate);

	// TODO: Determine optimal quality/performance settings (i.e. quickseek,
	// anti-aliasing, etc).

	soundtouch_setTempo(st, 0.1f);
	soundtouch_putSamples(st, _st_src, _NUM_ST_SRC_FRAMES);
	soundtouch_clear(st);
	soundtouch_setTempo(st, 1.0f);
}

ma_uint64 AudioPlayhead::_pull(unsigned int track_idx, AudioClip &clip,
	float *dest, ma_uint64 num_frames)
{
	ma_uint64 initial_want_frame = _cache[track_idx].cur_want_frame;
	ma_uint64 num_pulled = clip.pull_preload(dest, initial_want_frame,
		num_frames);

	_ChunksList &chunks = _cache[track_idx];
	while (chunks.head && num_pulled < num_frames) {
		PlayheadChunk *chunk = chunks.head;

		ma_uint64 cur_first_frame = initial_want_frame + num_pulled;
		ma_uint64 chunk_last_frame = chunk->first_frame +
			chunk->num_frames;

		if (chunk->song_id == _cur_song_ids[track_idx] &&
			cur_first_frame >= chunk->first_frame &&
			cur_first_frame < chunk_last_frame) {
			ma_uint64 need_frames = num_frames - num_pulled;
			ma_uint64 avail_frames = chunk_last_frame -
				cur_first_frame;
			ma_uint64 cur_src_first_frame = cur_first_frame -
				chunk->first_frame;
			ma_uint64 cur_dest_first_frame = num_pulled;

			if (avail_frames <= need_frames) {
				_copy_frames(dest, chunk->frames,
					cur_dest_first_frame,
					cur_src_first_frame, avail_frames,
					chunk->num_channels);
				num_pulled += avail_frames;

				_pop_chunk(chunks);
			} else {
				_copy_frames(dest, chunk->frames,
					cur_dest_first_frame,
					cur_src_first_frame, need_frames,
					chunk->num_channels);
				num_pulled += need_frames;
			}
		} else {
			_pop_chunk(chunks);
		}
	}

	_cache[track_idx].cur_want_frame = initial_want_frame + num_frames;

	return num_pulled;
}

void AudioPlayhead::_copy_frames(float *dest, float *src,
	ma_uint64 dest_first_frame, ma_uint64 src_first_frame,
	ma_uint64 num_frames, ma_uint64 num_channels)
{
	for (ma_uint64 i = 0; i < num_frames; ++i) {
		for (ma_uint64 j = 0; j < num_channels; ++j) {
			ma_uint64 src_idx = (src_first_frame + i) * num_channels
				+ j;
			ma_uint64 dest_idx = (dest_first_frame + i) *
				num_channels + j;

			dest[dest_idx] = src[src_idx];
		}
	}
}

void AudioPlayhead::_delete_chunk(PlayheadChunk *chunk)
{
	if (_io) {
		_io->delete_playhead_chunk(chunk);
	} else {
		if (chunk) {
			if (chunk->frames) {
				delete[] chunk->frames;
			}

			delete chunk;
		}
	}
}

void AudioPlayhead::_pop_chunk(_ChunksList &chunks)
{
	if (chunks.head) {
		if (chunks.head == chunks.tail) {
			_delete_chunk(chunks.head);
			chunks.head = nullptr;
			chunks.tail = nullptr;
		} else {
			PlayheadChunk *old_chunk = chunks.head;
			chunks.head = chunks.head->next;
			_delete_chunk(old_chunk);
		}
	}
}

void AudioPlayhead::_pop_all_chunks(unsigned int track_idx)
{
	_ChunksList &chunks = _cache[track_idx];
	while (chunks.head) {
		_pop_chunk(chunks);
	}
	chunks.head = nullptr;
	chunks.tail = nullptr;
}

bool AudioPlayhead::_needs_pop_and_seek(unsigned int track_idx,
	AudioClip &cur_clip)
{
	bool needs_pop_seek = true;

	if (_library && _is_track_valid(track_idx) &&
		_last_clips_valid[track_idx]) {
		_LastClipInfo &last_clip = _last_clips[track_idx];

		if (cur_clip.song_id == last_clip.song_id) {
			ma_uint64 frames_delta = llabs(static_cast<ma_int64>(
				cur_clip.first_frame) - static_cast<ma_int64>(
					last_clip.first_frame));

			double beats_delta = fabs(cur_clip.start -
				last_clip.start);

			ma_uint64 beats_delta_to_frames =
				_library->beats_to_out_samples(cur_clip.song_id,
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
				needs_pop_seek = false;
			}
		}
	}

	_last_clips[track_idx].start = cur_clip.start;
	_last_clips[track_idx].first_frame = cur_clip.first_frame;
	_last_clips[track_idx].song_id = cur_clip.song_id;
	_last_clips_valid[track_idx] = true;

	return needs_pop_seek;
}

bool AudioPlayhead::_is_track_valid(unsigned int track_idx)
{
	return track_idx < _NUM_TRACKS;
}
}
