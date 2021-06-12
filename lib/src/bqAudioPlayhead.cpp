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

	_st_src = new float[static_cast<ma_uint64>(_NUM_ST_SRC_FRAMES) *
		num_channels];

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

bool AudioPlayhead::pull_stretch(double master_bpm, unsigned int track_idx,
	AudioClip &clip, double song_bpm, float *dest, ma_uint64 first_frame,
	ma_uint64 num_frames, ma_uint64 next_expected_first_frame)
{
	if (!_is_track_valid(track_idx)) {
		return 0;
	}

	HANDLE st = _st[track_idx];

	if (_off_by_over_two(first_frame, _expect_first_frame[track_idx]) ||
		clip.song_id != _last_song_id[track_idx] ||
		!_last_song_id_valid[track_idx]) {
		soundtouch_clear(st);
		_cache[track_idx].cur_want_frame = first_frame;
	}

	_TrackStInfo &st_info = _st_info[track_idx];
	if (!st_info.valid || st_info.last_pitch != clip.pitch_shift) {
		soundtouch_setPitchSemiTones(st, static_cast<float>(
			clip.pitch_shift));
		st_info.last_pitch = clip.pitch_shift;
	}
	double tempo_shift = master_bpm / song_bpm;
	if (!st_info.valid || st_info.last_tempo != tempo_shift) {
		soundtouch_setTempo(st, static_cast<float>(tempo_shift));
		st_info.last_tempo = tempo_shift;
	}
	st_info.valid = true;

	bool all_pulls_successful = true;

	ma_uint64 total_num_received = 0;
	while (total_num_received < num_frames) {
		if (soundtouch_numSamples(st) == 0) {
			all_pulls_successful = _pull(track_idx, clip, _st_src,
				_NUM_ST_SRC_FRAMES) && all_pulls_successful;
			soundtouch_putSamples(st, _st_src, _NUM_ST_SRC_FRAMES);
		}

		unsigned int max_num_receive = static_cast<unsigned int>(
			num_frames - total_num_received);
		float *cur_dest = dest + (total_num_received * _num_channels);
		unsigned int cur_num_received = soundtouch_receiveSamples(st,
			cur_dest, max_num_receive);
		total_num_received += cur_num_received;
	}

	_expect_first_frame[track_idx] = next_expected_first_frame;
	_last_song_id[track_idx] = clip.song_id;
	_last_song_id_valid[track_idx] = true;

	return all_pulls_successful;
}

void AudioPlayhead::receive_chunk(unsigned int track_idx, PlayheadChunk *chunk)
{
	if (_is_track_valid(track_idx)) {
		_ChunksList &chunks = _cache[track_idx];
		if (chunks.tail) {
			chunks.tail->next = chunk;
			chunks.tail = chunk;
		} else {
			chunks.head = chunk;
			chunks.tail = chunk;
		}
		_can_request_emergency_chunk[track_idx] = true;
	} else {
		_delete_chunk(chunk);
	}
}

double AudioPlayhead::get_beat()
{
	return _beat;
}

ma_uint64 AudioPlayhead::get_cur_want_frame(unsigned int track_idx)
{
	if (_is_track_valid(track_idx)) {
		return _cache[track_idx].cur_want_frame;
	} else {
		return 0;
	}
}

void AudioPlayhead::jump(double beat)
{
	if (beat != _beat) {
		for (unsigned int i = 0; i < _NUM_TRACKS; ++i) {
			set_cannot_request_emergency_chunk(i);
			_pop_all_chunks(i);
		}
		_beat = beat;
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
		cur_clip_idx = _cur_clip_idx[track_idx];
	}

	return cur_clip_idx;
}

unsigned int AudioPlayhead::get_cur_song_id(unsigned int track_idx)
{
	unsigned int cur_song_id = 0;

	if (_is_track_valid(track_idx)) {
		cur_song_id = _cur_song_id[track_idx];
	}

	return cur_song_id;
}

void AudioPlayhead::set_cur_clip_idx(unsigned int track_idx,
	unsigned int cur_clip_idx)
{
	if (_is_track_valid(track_idx)) {
		_cur_clip_idx[track_idx] = cur_clip_idx;
	}
}

void AudioPlayhead::set_cur_song_id(unsigned int track_idx,
	unsigned int cur_song_id)
{
	if (_is_track_valid(track_idx)) {
		_cur_song_id[track_idx] = cur_song_id;
	}
}

void AudioPlayhead::set_cannot_request_emergency_chunk(unsigned int track_idx)
{
	if (_is_track_valid(track_idx)) {
		_can_request_emergency_chunk[track_idx] = false;
	}
}

bool AudioPlayhead::get_can_request_emergency_chunk(unsigned int track_idx)
{
	if (_is_track_valid(track_idx)) {
		return _can_request_emergency_chunk[track_idx];
	} else {
		return false;
	}
}

void AudioPlayhead::_setup_soundtouch(HANDLE &st)
{
	if (st) {
		soundtouch_clear(st);
	} else {
		st = soundtouch_createInstance();

		// 2 = SETTING_USE_QUICKSEEK
		soundtouch_setSetting(st, 2,
			bq::TIMESTRETCH_USE_QUICKSEEK ? 1 : 0);
		// 3 = SETTING_SEQUENCE_MS
		soundtouch_setSetting(st, 3, bq::TIMESTRETCH_SEQUENCE_MS);
		// 4 = SETTING_SEEKWINDOW_MS
		soundtouch_setSetting(st, 4, bq::TIMESTRETCH_SEEKWINDOW_MS);
		// 5 = SETTING_OVERLAP_MS
		soundtouch_setSetting(st, 5, bq::TIMESTRETCH_OVERLAP_MS);
	}

	soundtouch_setChannels(st, _num_channels);
	soundtouch_setSampleRate(st, _sample_rate);

	soundtouch_setTempo(st, 0.1f);
	soundtouch_putSamples(st, _st_src, _NUM_ST_SRC_FRAMES);
	soundtouch_clear(st);
	soundtouch_setTempo(st, 1.0f);

	_st_info->valid = false;
}

bool AudioPlayhead::_pull(unsigned int track_idx, AudioClip &clip, float *dest,
	ma_uint64 num_frames)
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

		if (chunk->song_id == clip.song_id &&
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

	if (num_pulled < num_frames) {
		// Passing silence to SoundTouch when we don't have any data is
		// necessary to keeping multiple tracks in sync (so that the
		// latency between each SoundTouch instance is consistent even
		// if some tracks' clips decoded more quickly than others).
		// Thus, if we don't have enough samples to fill the whole
		// buffer, we fill the remaining part with silence.
		_fill_silence(dest, num_pulled, num_frames - num_pulled,
			_num_channels);

		return false;
	}

	return true;
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

void AudioPlayhead::_fill_silence(float *dest, ma_uint64 first_frame,
	ma_uint64 num_frames, ma_uint64 num_channels)
{
	ma_uint64 ofs = first_frame * num_channels;
	for (ma_uint64 i = 0; i < num_frames * num_channels; ++i) {
		dest[ofs + i] = 0.0f;
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

bool AudioPlayhead::_off_by_over_two(ma_uint64 a, ma_uint64 b)
{
	return std::llabs(static_cast<ma_int64>(a - b)) > 2;
}

bool AudioPlayhead::_is_track_valid(unsigned int track_idx)
{
	return track_idx < _NUM_TRACKS;
}
}
