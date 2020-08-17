#ifndef BQAUDIOPLAYHEAD_H
#define BQAUDIOPLAYHEAD_H

#include "bqAudioClip.h"
#include "bqPlayheadChunk.h"
#include "bqLibrary.h"
#include "bqConfig.h"

#include <miniaudio.h>
#include <SoundTouchDLL.h>

#include <atomic>

namespace bq {
class IOEngine;

class AudioPlayhead {
public:
	AudioPlayhead() {}
	~AudioPlayhead();

	void set_playback_config(ma_uint32 num_channels, ma_uint32 sample_rate);
	void bind_io_engine(IOEngine *io);
	void bind_library(Library *library);

	ma_uint64 pull_stretch(double master_bpm, unsigned int track_idx,
		AudioClip &clip, double song_bpm, float *dest,
		ma_uint64 first_frame, ma_uint64 num_frames,
		ma_uint64 next_expected_first_frame);

	void receive_chunk(unsigned int track, PlayheadChunk *chunk);

	double get_beat();
	ma_uint64 get_cur_want_frame(unsigned int track_idx);
	void advance(double beats);
	// Causes free; must only be called within the audio thread
	void jump(double beat);

	unsigned int get_cur_clip_idx(unsigned int track_idx);
	unsigned int get_cur_song_id(unsigned int track_idx);
	void set_cur_clip_idx(unsigned int track_idx,
		unsigned int cur_clip_idx);
	void set_cur_song_id(unsigned int track_idx, unsigned int cur_song_id);

private:
	void _setup_soundtouch(HANDLE &st);

	void _pull(unsigned int track_idx, AudioClip &clip, float *dest,
		ma_uint64 num_frames);

	static constexpr unsigned int _NUM_TRACKS = WORLD_NUM_TRACKS;
	struct _TrackStInfo {
		double last_pitch = 0.0;
		double last_tempo = 0.0;
		bool valid = false;
	};
	struct _ChunksList {
		PlayheadChunk *head = nullptr, *tail = nullptr;
		std::atomic<ma_uint64> cur_want_frame = 0;
	};

	void _copy_frames(float *dest, float *src, ma_uint64 dest_first_frame,
		ma_uint64 src_first_frame, ma_uint64 num_frames,
		ma_uint64 num_channels);
	void _fill_silence(float *dest, ma_uint64 first_frame,
		ma_uint64 num_frames, ma_uint64 num_channels);

	void _delete_chunk(PlayheadChunk *chunk);
	void _pop_chunk(_ChunksList &chunks);
	void _pop_all_chunks(unsigned int track_idx);

	bool _is_track_valid(unsigned int track_idx);

	std::atomic<double> _beat = 0.0;

	// Thank you Mixxx developers for figuring this number out!
	// https://github.com/mixxxdj/mixxx/blob/master/src/engine/bufferscalers/enginebufferscalest.cpp#L25
	//
	// TODO: At some point, perhaps setting this to a value higher than
	// necessary (to ensure _st_src is allocated large enough), and then
	// using SETTING_INITIAL_LATENCY (if SoundTouch instance was cleared) or
	// SETTING_NOMINAL_INPUT_SEQUENCE (if SoundTouch instance was not
	// cleared) inside pull_stretch's while loop, rather than
	// _NUM_ST_SRC_FRAMES, would be optimal.
	static constexpr unsigned int _NUM_ST_SRC_FRAMES = 519;
	float *_st_src = nullptr;

	HANDLE _st[_NUM_TRACKS] = { nullptr };

	_TrackStInfo _st_info[_NUM_TRACKS];
	_ChunksList _cache[_NUM_TRACKS];
	std::atomic<unsigned int> _cur_clip_idx[_NUM_TRACKS] = { 0 };
	std::atomic<unsigned int> _cur_song_id[_NUM_TRACKS] = { 0 };
	std::atomic<ma_uint64> _expect_first_frame[_NUM_TRACKS] = { 0 };
	std::atomic<unsigned int> _last_song_id[_NUM_TRACKS] = { 0 };
	std::atomic<bool> _last_song_id_valid[_NUM_TRACKS] = { false };

	ma_uint32 _num_channels = 0;
	ma_uint32 _sample_rate = 0;

	IOEngine *_io = nullptr;
	Library *_library = nullptr;
};
}

#endif
