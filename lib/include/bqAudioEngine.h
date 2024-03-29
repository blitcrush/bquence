#ifndef BQAUDIOENGINE_H
#define BQAUDIOENGINE_H

#include "bqAudioPlayhead.h"
#include "bqPlayheadChunk.h"
#include "bqOldPreloadsArray.h"
#include "bqAudioClipsArray.h"
#include "bqAudioMsg.h"
#include "bqLibrary.h"
#include "bqConfig.h"

#include <miniaudio.h>

#include <QwMpscFifoQueue.h>
#include <QwNodePool.h>

#include <cmath>

namespace bq {
class IOEngine;

class AudioEngine {
public:
	AudioEngine();
	~AudioEngine();

	void pull(unsigned int playhead_idx, unsigned int track_idx,
		float *dest, ma_uint64 num_frames);
	void pull_done_advance_playhead(unsigned int playhead_idx,
		ma_uint64 num_frames);

	void set_playback_config(ma_uint32 num_channels, ma_uint32 sample_rate);
	void bind_io_engine(IOEngine *io);
	void bind_library(Library *library);

	void handle_all_msgs();

	void receive_clips(unsigned int track, AudioClipsArray clips);
	void receive_old_preloads(OldPreloadsArray old_preloads);
	void receive_cur_clip_idx(unsigned int playhead, unsigned int track,
		unsigned int cur_clip_idx);
	void receive_playhead_chunk(unsigned int playhead, unsigned int track,
		PlayheadChunk *chunk);

	// AudioPlayhead uses only atomic variables to track this state, so we
	// don't need to pass messages here...
	double get_playhead_beat(unsigned int playhead_idx);
	ma_uint64 get_cur_want_frame(unsigned int playhead_idx,
		unsigned int track_idx);
	unsigned int get_playhead_cur_clip_idx(unsigned int playhead_idx,
		unsigned int track_idx);
	unsigned int get_playhead_cur_song_id(unsigned int playhead_idx,
		unsigned int track_idx);
	// However, we want to pass messages in this case, because we need to be
	// sure this is processed after any track updates (so indices are valid)
	void jump_playhead(unsigned int playhead_idx, unsigned int track_idx,
		unsigned int cur_clip_idx, double beat);

	double get_bpm();
	void set_bpm(double bpm);
	ma_uint64 beats_to_samples(double beats);
	double samples_to_beats(double samples);

private:
	void _fade(float *dest, ma_uint64 dest_num_frames,
		float total_num_frames, float initial_x, bool reverse);
	float _sigmoid_0_to_1(float x);

	void _apply_next_bpm();
	void _recalc_beats_samples_conversion_factors();

	bool _is_track_valid(unsigned int track_idx);
	bool _is_playhead_valid(unsigned int playhead_idx);

	void _handle_receive_clips(AudioMsgReceiveClips &msg);
	void _handle_receive_old_preloads(AudioMsgReceiveOldPreloads &msg);
	void _handle_receive_cur_clip_idx(AudioMsgReceiveCurClipIdx &msg);
	void _handle_receive_playhead_chunk(AudioMsgReceivePlayheadChunk &msg);
	void _handle_jump_playhead(AudioMsgJumpPlayhead &msg);

	void _update_cur_clip_idx(unsigned int playhead_idx,
		unsigned int track_idx);

	void _fill_silence(float *dest, ma_uint64 first_frame,
		ma_uint64 num_frames, ma_uint64 num_channels);

	template<class T>
	constexpr const T &_min(const T &a, const T &b)
	{
		return (a < b) ? a : b;
	}

	template<class T>
	constexpr const T &_clamp(const T &x, const T &a, const T &b)
	{
		return (x < a) ? a : (x > b) ? b : x;
	}

	std::atomic<unsigned int> _num_channels;
	std::atomic<unsigned int> _sample_rate;
	std::atomic<double> _bpm, _next_bpm;
	std::atomic<double> _beats_to_samples;
	std::atomic<double> _samples_to_beats;

	AudioClipsArray _tracks[WORLD_NUM_TRACKS];

	AudioPlayhead _playheads[WORLD_NUM_PLAYHEADS];

	QwMpscFifoQueue<AudioMsg *, AUDIO_MSG_NEXT_LINK> _msg_queue;
	QwNodePool<AudioMsg> *_msg_pool = nullptr;

	IOEngine *_io = nullptr;
	Library *_library = nullptr;

	static constexpr unsigned int _NUM_MAX_POOL_MSGS =
		ENGINE_MAX_NUM_POOL_MSGS;
};
}

#endif
