#include "bqAudioEngine.h"
#include "bqIOEngine.h"

namespace bq {
AudioEngine::AudioEngine()
{
	set_bpm(120.0);

	_msg_pool = new QwNodePool<AudioMsg>(_NUM_MAX_POOL_MSGS);

	for (unsigned int i = 0; i < _NUM_TRACKS; ++i) {
		_tracks[i].num_clips = 0;
		_tracks[i].clips = nullptr;
	}
}

AudioEngine::~AudioEngine()
{
	// The Library and IOEngine might be destroyed in this situation, for
	// example if the program is exiting and so destructors are being
	// called. In this situation, if this AudioEngine instance still knew
	// about its library and IOEngine, it might send them messages or
	// request data from them even though they are being destroyed, which
	// would cause memory management errors.
	bind_library(nullptr);
	bind_io_engine(nullptr);

	handle_all_msgs();

	for (unsigned int i = 0; i < _NUM_TRACKS; ++i) {
		if (_tracks[i].clips) {
			delete[] _tracks[i].clips;
		}
		_tracks[i].num_clips = 0;
	}

	delete _msg_pool;
}

void AudioEngine::pull(unsigned int playhead_idx, unsigned int track_idx,
	float *dest, ma_uint64 num_frames)
{
	if (!_library || !_is_playhead_valid(playhead_idx) ||
		!_is_track_valid(track_idx)) {
		_fill_silence(dest, 0, num_frames, _num_channels);
		return;
	}

	AudioPlayhead &playhead = _playheads[playhead_idx];
	AudioClipsArray &track = _tracks[track_idx];

	if (track.num_clips < 1) {
		_fill_silence(dest, 0, num_frames, _num_channels);
		return;
	}

	_update_cur_clip_idx(playhead_idx, track_idx);

	double num_beats = samples_to_beats(static_cast<double>(num_frames));
	double first_beat = playhead.get_beat();
	double last_beat = first_beat + num_beats;

	unsigned int first_clip = playhead.get_cur_clip_idx(track_idx);
	if (!track.is_clip_valid(first_clip)) {
		_fill_silence(dest, 0, num_frames, _num_channels);
		return;
	}
	unsigned int last_clip = first_clip;

	while (track.is_clip_valid(last_clip + 1)) {
		if (track.clips[last_clip + 1].start < last_beat) {
			++last_clip;
		} else {
			break;
		}
	}

	ma_uint64 prev_last_frame_ofs = 0;
	for (unsigned int i = first_clip; i <= last_clip; ++i) {
		AudioClip &clip = track.clips[i];

		if (clip.end <= first_beat || clip.start >= last_beat) {
			continue;
		}

		double clip_first_beat = clip.start;
		if (clip_first_beat < first_beat) {
			clip_first_beat = first_beat;
		}

		double clip_last_beat = clip.end;
		if (clip_last_beat > last_beat) {
			clip_last_beat = last_beat;
		}

		ma_uint64 clip_first_frame_ofs = beats_to_samples(
			clip_first_beat - first_beat);
		ma_uint64 clip_last_frame_ofs = beats_to_samples(
			clip_last_beat - first_beat);
		// In case beats_to_samples rounded up past the last available
		// frame we can fill...
		if (clip_last_frame_ofs > num_frames) {
			clip_last_frame_ofs = num_frames;
		}

		ma_uint64 clip_num_frames = clip_last_frame_ofs -
			clip_first_frame_ofs;
		if (clip_num_frames <= 0) {
			continue;
		}

		double song_bpm = _library->bpm(clip.song_id);
		if (song_bpm <= 0.0) { // Song ID is invalid...
			continue;
		}
		ma_uint64 song_first_frame = clip.first_frame +
			_library->beats_to_out_samples(clip.song_id,
				first_beat - clip.start);
		ma_uint64 song_next_first_frame = clip.first_frame +
			_library->beats_to_out_samples(clip.song_id,
				last_beat - clip.start);

		if (prev_last_frame_ofs < clip_first_frame_ofs) {
			ma_uint64 silence_num_frames = clip_first_frame_ofs -
				prev_last_frame_ofs;
			_fill_silence(dest, prev_last_frame_ofs,
				silence_num_frames, _num_channels);
		}

		float *pull_dest = dest + (clip_first_frame_ofs *
			_num_channels);
		_playheads[playhead_idx].pull_stretch(_bpm, track_idx, i, clip,
			song_bpm, pull_dest, song_first_frame,
			clip_num_frames, song_next_first_frame);

		// Fade in
		double fade_in_last_beat = clip.start + clip.fade_in;
		if (first_beat < fade_in_last_beat) {
			ma_uint64 fade_num_frames = beats_to_samples(
				clip.fade_in);
			ma_uint64 dest_num_frames = _min(clip_num_frames,
				fade_num_frames);
			float *fade_dest = dest + (clip_first_frame_ofs *
				_num_channels);
			float fade_base = static_cast<float>(
				(first_beat - clip.start) / clip.fade_in);
			_fade(fade_dest, dest_num_frames,
				static_cast<float>(fade_num_frames), fade_base,
				false);
		}

		// Fade out
		double fade_out_first_beat = clip.end - clip.fade_out;
		if (last_beat > fade_out_first_beat) {
			ma_uint64 fade_num_frames = beats_to_samples(
				clip.fade_out);
			ma_uint64 dest_num_frames = _min(clip_num_frames,
				fade_num_frames);
			float *fade_dest = dest +
				((clip_last_frame_ofs - dest_num_frames) *
					_num_channels);
			float fade_base = static_cast<float>(
				(clip.end - first_beat) / clip.fade_out);
			_fade(fade_dest, dest_num_frames,
				static_cast<float>(fade_num_frames), fade_base,
				true);
		}

		prev_last_frame_ofs = clip_last_frame_ofs;
	}
	if (prev_last_frame_ofs < num_frames) {
		ma_uint64 silence_num_frames = num_frames -
			prev_last_frame_ofs;
		_fill_silence(dest, prev_last_frame_ofs,
			silence_num_frames, _num_channels);
	}
}

void AudioEngine::pull_done_advance_playhead(unsigned int playhead_idx,
	ma_uint64 num_frames)
{
	_playheads[playhead_idx].advance(samples_to_beats(
		static_cast<double>(num_frames)));
}

void AudioEngine::set_playback_config(ma_uint32 num_channels,
	ma_uint32 sample_rate)
{
	_num_channels = num_channels;
	_sample_rate = sample_rate;
	_recalc_beats_samples_conversion_factors();

	for (unsigned int i = 0; i < _NUM_PLAYHEADS; ++i) {
		_playheads[i].set_playback_config(num_channels, sample_rate);
	}
}

void AudioEngine::bind_io_engine(IOEngine *io)
{
	_io = io;

	for (unsigned int i = 0; i < _NUM_PLAYHEADS; ++i) {
		_playheads[i].bind_io_engine(_io);
	}
}

void AudioEngine::bind_library(Library *library)
{
	_library = library;

	for (unsigned int i = 0; i < _NUM_PLAYHEADS; ++i) {
		_playheads[i].bind_library(_library);
	}
}

void AudioEngine::handle_all_msgs()
{
	AudioMsg *msg = nullptr;
	while (msg = _msg_queue.pop()) {
		switch (msg->type) {
		case AudioMsgType::RECEIVE_CLIPS:
			_handle_receive_clips(msg->contents.receive_clips);
			break;

		case AudioMsgType::RECEIVE_OLD_PRELOADS:
			_handle_receive_old_preloads(
				msg->contents.receive_old_preloads);
			break;

		case AudioMsgType::RECEIVE_CUR_CLIP_IDX:
			_handle_receive_cur_clip_idx(
				msg->contents.receive_cur_clip_idx);
			break;

		case AudioMsgType::RECEIVE_PLAYHEAD_CHUNK:
			_handle_receive_playhead_chunk(
				msg->contents.receive_playhead_chunk);
			break;

		case AudioMsgType::JUMP_PLAYHEAD:
			_handle_jump_playhead(msg->contents.jump_playhead);
			break;

		default:
			break;
		}

		_msg_pool->deallocate(msg);
	}
}

void AudioEngine::receive_clips(unsigned int track, AudioClipsArray clips)
{
	AudioMsg *msg = _msg_pool->allocate();
	msg->type = AudioMsgType::RECEIVE_CLIPS;
	msg->contents.receive_clips.track = track;
	msg->contents.receive_clips.clips = clips;
	_msg_queue.push(msg);
}

void AudioEngine::receive_old_preloads(OldPreloadsArray old_preloads)
{
	AudioMsg *msg = _msg_pool->allocate();
	msg->type = AudioMsgType::RECEIVE_OLD_PRELOADS;
	msg->contents.receive_old_preloads.old_preloads = old_preloads;
	_msg_queue.push(msg);
}

void AudioEngine::receive_cur_clip_idx(unsigned int playhead,
	unsigned int track, unsigned int cur_clip_idx)
{
	AudioMsg *msg = _msg_pool->allocate();
	msg->type = AudioMsgType::RECEIVE_CUR_CLIP_IDX;
	msg->contents.receive_cur_clip_idx.playhead = playhead;
	msg->contents.receive_cur_clip_idx.track = track;
	msg->contents.receive_cur_clip_idx.cur_clip_idx = cur_clip_idx;
	_msg_queue.push(msg);
}

void AudioEngine::receive_playhead_chunk(unsigned int playhead,
	unsigned int track, PlayheadChunk *chunk)
{
	AudioMsg *msg = _msg_pool->allocate();
	msg->type = AudioMsgType::RECEIVE_PLAYHEAD_CHUNK;
	msg->contents.receive_playhead_chunk.playhead = playhead;
	msg->contents.receive_playhead_chunk.track = track;
	msg->contents.receive_playhead_chunk.chunk = chunk;
	_msg_queue.push(msg);
}

double AudioEngine::get_playhead_beat(unsigned int playhead_idx)
{
	if (_is_playhead_valid(playhead_idx)) {
		return _playheads[playhead_idx].get_beat();
	} else {
		return -1.0;
	}
}

ma_uint64 AudioEngine::get_cur_want_frame(unsigned int playhead_idx,
	unsigned int track_idx)
{
	if (_is_playhead_valid(playhead_idx)) {
		return _playheads[playhead_idx].get_cur_want_frame(track_idx);
	} else {
		return 0;
	}
}

unsigned int AudioEngine::get_playhead_cur_clip_idx(unsigned int playhead_idx,
	unsigned int track_idx)
{
	if (_is_playhead_valid(playhead_idx)) {
		return _playheads[playhead_idx].get_cur_clip_idx(track_idx);
	} else {
		return 0;
	}
}

unsigned int AudioEngine::get_playhead_cur_song_id(unsigned int playhead_idx,
	unsigned int track_idx)
{
	if (_is_playhead_valid(playhead_idx)) {
		return _playheads[playhead_idx].get_cur_song_id(track_idx);
	} else {
		return 0;
	}
}

void AudioEngine::jump_playhead(unsigned int playhead_idx,
	unsigned int track_idx, unsigned int cur_clip_idx, double beat)
{
	AudioMsg *msg = _msg_pool->allocate();
	msg->type = AudioMsgType::JUMP_PLAYHEAD;
	msg->contents.jump_playhead.playhead = playhead_idx;
	msg->contents.jump_playhead.track = track_idx;
	msg->contents.jump_playhead.cur_clip_idx = cur_clip_idx;
	msg->contents.jump_playhead.beat = beat;
	_msg_queue.push(msg);
}

double AudioEngine::get_bpm()
{
	return _bpm;
}

void AudioEngine::set_bpm(double bpm)
{
	_bpm = bpm;
	_recalc_beats_samples_conversion_factors();
}

ma_uint64 AudioEngine::beats_to_samples(double beats)
{
	if (beats < 0.0) {
		beats = -beats;
	}

	// + 0.5 is a cheap way to round when we don't have to worry about
	// negative numbers (which we don't, because this is unsigned)
	return static_cast<ma_uint64>(beats * _beats_to_samples + 0.5);
}

double AudioEngine::samples_to_beats(double samples)
{
	return samples * _samples_to_beats;
}

void AudioEngine::_fade(float *dest, ma_uint64 dest_num_frames,
	float total_num_frames, float initial_x, bool reverse)
{
	for (ma_uint64 i = 0; i < dest_num_frames; ++i) {
		float x = static_cast<float>(i) / total_num_frames;
		x = reverse ? initial_x - x : initial_x + x;
		x = _clamp(_sigmoid_0_to_1(x), 0.0f, 1.0f);
		for (ma_uint64 j = 0; j < _num_channels; ++j) {
			dest[i * _num_channels + j] *= x;
		}
	}
}

float AudioEngine::_sigmoid_0_to_1(float x)
{
	x -= 0.5f;
	return 0.5f + 1.5f * (x / (1.0f + abs(x)));
}

void AudioEngine::_recalc_beats_samples_conversion_factors()
{
	_beats_to_samples = (60.0 / _bpm) * _sample_rate;
	_samples_to_beats = 1.0 / _beats_to_samples;
}

bool AudioEngine::_is_track_valid(unsigned int track_idx)
{
	return track_idx < _NUM_TRACKS;
}

bool AudioEngine::_is_playhead_valid(unsigned int playhead_idx)
{
	return playhead_idx < _NUM_PLAYHEADS;
}

void AudioEngine::_handle_receive_clips(AudioMsgReceiveClips &msg)
{
	if (_is_track_valid(msg.track)) {
		if (_io) {
			_io->delete_clips(_tracks[msg.track]);
		} else {
			if (_tracks[msg.track].clips) {
				delete[] _tracks[msg.track].clips;
				_tracks[msg.track].clips = nullptr;
			}
			_tracks[msg.track].num_clips = 0;
		}

		_tracks[msg.track] = msg.clips;
	} else {
		if (_io) {
			_io->delete_clips(msg.clips);
		} else {
			if (msg.clips.clips) {
				delete[] msg.clips.clips;
			}
		}
	}
}

void AudioEngine::_handle_receive_old_preloads(AudioMsgReceiveOldPreloads &msg)
{
	if (_io) {
		_io->delete_old_preloads(msg.old_preloads);
	} else {
		if (msg.old_preloads.frames) {
			for (unsigned int i = 0;
				i < msg.old_preloads.num_preloads; ++i) {
				if (msg.old_preloads.frames[i]) {
					delete[] msg.old_preloads.frames[i];
				}
			}

			delete[] msg.old_preloads.frames;
		}
	}
}

void AudioEngine::_handle_receive_cur_clip_idx(AudioMsgReceiveCurClipIdx &msg)
{
	if (_is_playhead_valid(msg.playhead) && _is_track_valid(msg.track)) {
		AudioPlayhead &playhead = _playheads[msg.playhead];
		AudioClipsArray &track = _tracks[msg.track];

		if (track.is_clip_valid(msg.cur_clip_idx)) {
			AudioClip &cur_clip = track.clips[msg.cur_clip_idx];
			playhead.set_cur_clip_idx(msg.track, msg.cur_clip_idx);
			playhead.set_cur_song_id(msg.track, cur_clip.song_id);
		}
	}
}

void AudioEngine::_handle_receive_playhead_chunk(
	AudioMsgReceivePlayheadChunk &msg)
{
	if (_is_playhead_valid(msg.playhead) && _is_track_valid(msg.track)) {
		_playheads[msg.playhead].receive_chunk(msg.track, msg.chunk);
	} else {
		if (_io) {
			_io->delete_playhead_chunk(msg.chunk);
		} else {
			if (msg.chunk->frames) {
				delete[] msg.chunk->frames;
			}

			delete msg.chunk;
		}
	}
}

void AudioEngine::_handle_jump_playhead(AudioMsgJumpPlayhead &msg)
{
	if (_is_playhead_valid(msg.playhead) && _is_track_valid(msg.track)) {
		AudioPlayhead &playhead = _playheads[msg.playhead];
		AudioClipsArray &track = _tracks[msg.track];

		playhead.jump(msg.beat);

		if (track.is_clip_valid(msg.cur_clip_idx)) {
			AudioClip &cur_clip = track.clips[msg.cur_clip_idx];
			playhead.set_cur_clip_idx(msg.track, msg.cur_clip_idx);
			playhead.set_cur_song_id(msg.track, cur_clip.song_id);
		}

		if (_io) {
			_io->notify_audio_playhead_jumped(msg.playhead);
		}
	}
}

void AudioEngine::_update_cur_clip_idx(unsigned int playhead_idx,
	unsigned int track_idx)
{
	AudioPlayhead &playhead = _playheads[playhead_idx];
	AudioClipsArray &track = _tracks[track_idx];

	double playhead_beat = playhead.get_beat();
	unsigned int old_cur_clip_idx = playhead.get_cur_clip_idx(track_idx);

	unsigned int cur_clip_idx = 0;
	for (unsigned int i = old_cur_clip_idx; i < track.num_clips; ++i) {
		if (track.clips[i].start <= playhead_beat) {
			cur_clip_idx = i;
		} else {
			break;
		}
	}

	if (track.is_clip_valid(cur_clip_idx)) {
		AudioClip &cur_clip = track.clips[cur_clip_idx];
		playhead.set_cur_clip_idx(track_idx, cur_clip_idx);
		playhead.set_cur_song_id(track_idx, cur_clip.song_id);
	}
}

void AudioEngine::_fill_silence(float *dest, ma_uint64 first_frame,
	ma_uint64 num_frames, ma_uint64 num_channels)
{
	ma_uint64 ofs = first_frame * num_channels;
	for (ma_uint64 i = 0; i < num_frames * num_channels; ++i) {
		dest[ofs + i] = 0.0f;
	}
}
}
