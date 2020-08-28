#ifndef BQIOENGINE_H
#define BQIOENGINE_H

#include "bqOldPreloadsArray.h"
#include "bqAudioClipsArray.h"
#include "bqIOTrack.h"
#include "bqIOMsg.h"
#include "bqIOAudioFileDecoder.h"
#include "bqLibrary.h"
#include "bqConfig.h"

#include <QwMpscFifoQueue.h>
#include <QwNodePool.h>

namespace bq {
class AudioEngine;

class IOEngine {
public:
	IOEngine();
	~IOEngine();

	void decode_next_cache_chunks(unsigned int playhead_idx,
		unsigned int track_idx);

	void set_decode_config(ma_uint32 num_channels, ma_uint32 sample_rate);
	void bind_audio_engine(AudioEngine *audio);
	void bind_library(Library *library);

	void handle_all_msgs();

	void insert_clip(unsigned int track, double start, double end,
		double fade_in, double fade_out, unsigned int song_id,
		ma_uint64 first_frame, double pitch_shift);
	void erase_clips_range(unsigned int track, double from, double to);

	void delete_clips(AudioClipsArray clips);
	void delete_old_preloads(OldPreloadsArray old_preloads);
	void delete_playhead_chunk(PlayheadChunk *chunk);

	void jump_playhead(unsigned int playhead, double beat);
	void notify_audio_playhead_jumped(unsigned int playhead);

	bool wait_cur_want_frame(unsigned int playhead, unsigned int track);
	void set_wait_cur_want_frame(unsigned int playhead, unsigned int track,
		bool value);

private:
	bool _is_track_valid(unsigned int track_idx);
	bool _is_playhead_valid(unsigned int playhead_idx);

	void _handle_insert_clip(IOMsgInsertClip &msg);
	void _handle_erase_clips_range(IOMsgEraseClipsRange &msg);

	void _handle_delete_clips(IOMsgDeleteClips &msg);
	void _handle_delete_old_preloads(IOMsgDeleteOldPreloads &msg);
	void _handle_delete_playhead_chunk(IOMsgDeletePlayheadChunk &msg);

	void _handle_jump_playhead(IOMsgJumpPlayhead &msg);
	void _handle_audio_playhead_jumped(IOMsgAudioPlayheadJumped &msg);

	void _update_audio_cur_clip_idx(unsigned int playhead_idx,
		unsigned int track_idx);

	static constexpr unsigned int _NUM_TRACKS = WORLD_NUM_TRACKS;
	IOTrack _tracks[_NUM_TRACKS];
	bool _track_dirty[_NUM_TRACKS] = { false };

	// Used to determine for how many playheads we need to update current
	// clip indices for the AudioEngine
	static constexpr unsigned int _NUM_PLAYHEADS = WORLD_NUM_PLAYHEADS;
	struct DirtyPlayheadInfo {
		double jump_beat = 0.0;
		bool jumping = false;
		bool wait_playhead_jump = false;
		bool cur_clip_dirty[_NUM_TRACKS] = { false };
	} _playheads[_NUM_PLAYHEADS];
	std::atomic_bool _wait_cur_want_frame[_NUM_PLAYHEADS][_NUM_TRACKS] =
	{ false };

	IOAudioFileDecoder _decoders[_NUM_PLAYHEADS][_NUM_TRACKS];

	QwMpscFifoQueue<IOMsg *, IO_MSG_NEXT_LINK> _msg_queue;
	QwNodePool<IOMsg> *_msg_pool = nullptr;

	ma_uint32 _decode_num_channels = 0, _decode_sample_rate = 0;

	AudioEngine *_audio = nullptr;
	Library *_library = nullptr;

	static constexpr unsigned int _NUM_MAX_POOL_MSGS =
		ENGINE_MAX_NUM_POOL_MSGS;
};
}

#endif
