#include "bqIOEngine.h"
#include "bqAudioEngine.h"

namespace bq {
IOEngine::IOEngine()
{
	_msg_pool = new QwNodePool<IOMsg>(_NUM_MAX_POOL_MSGS);
}

IOEngine::~IOEngine()
{
	// The Library and AudioEngine might be destroyed in this situation, for
	// example if the program is exiting and so destructors are being
	// called. In this situation, if this IOEngine instance still knew about
	// its library and AudioEngine, it might send them messages or request
	// data from them even though they are being destroyed, which would
	// cause memory management errors.
	bind_library(nullptr);
	bind_audio_engine(nullptr);

	handle_all_msgs();

	delete _msg_pool;
}

void IOEngine::decode_next_cache_chunks()
{
	if (!_library || !_audio) {
		return;
	}

	for (unsigned int i = 0; i < _NUM_PLAYHEADS; ++i) {
		for (unsigned int j = 0; j < _NUM_TRACKS; ++j) {
			if (_tracks[j].num_clips() > 0) {
				_decode_next_cache_chunks(i, j);
			}
		}
	}
}

void IOEngine::set_decode_config(ma_uint32 num_channels, ma_uint32 sample_rate)
{
	_decode_num_channels = num_channels;
	_decode_sample_rate = sample_rate;

	for (unsigned int i = 0; i < _NUM_TRACKS; ++i) {
		_tracks[i].set_preload_config(_decode_num_channels,
			_decode_sample_rate);

		for (unsigned int j = 0; j < _NUM_PLAYHEADS; ++j) {
			_decoders[j][i].set_decode_config(_decode_num_channels,
				_decode_sample_rate);
		}
	}
}

void IOEngine::bind_audio_engine(AudioEngine *audio)
{
	_audio = audio;
}

void IOEngine::bind_library(Library *library)
{
	_library = library;

	for (unsigned int i = 0; i < _NUM_TRACKS; ++i) {
		for (unsigned int j = 0; j < _NUM_PLAYHEADS; ++j) {
			_decoders[j][i].bind_library(_library);
		}

		_tracks[i].bind_library(_library);
	}
}

void IOEngine::handle_all_msgs()
{
	IOMsg *msg = nullptr;
	while (msg = _msg_queue.pop()) {
		switch (msg->type) {
		case IOMsgType::INSERT_CLIP:
			_handle_insert_clip(msg->contents.insert_clip);
			break;

		case IOMsgType::ERASE_CLIPS_RANGE:
			_handle_erase_clips_range(
				msg->contents.erase_clips_range);
			break;

		case IOMsgType::DELETE_CLIPS:
			_handle_delete_clips(msg->contents.delete_clips);
			break;

		case IOMsgType::DELETE_OLD_PRELOADS:
			_handle_delete_old_preloads(
				msg->contents.delete_old_preloads);
			break;

		case IOMsgType::DELETE_PLAYHEAD_CHUNK:
			_handle_delete_playhead_chunk(
				msg->contents.delete_playhead_chunk);
			break;

		case IOMsgType::JUMP_PLAYHEAD:
			_handle_jump_playhead(msg->contents.jump_playhead);
			break;

		case IOMsgType::AUDIO_PLAYHEAD_JUMPED:
			_handle_audio_playhead_jumped(
				msg->contents.audio_playhead_jumped);
			break;

		default:
			break;
		}

		_msg_pool->deallocate(msg);
	}

	for (unsigned int i = 0; i < _NUM_TRACKS; ++i) {
		if (_track_dirty[i]) {
			if (_audio) {
				_audio->receive_clips(i,
					_tracks[i].copy_clips());
				_audio->receive_old_preloads(
					_tracks[i].copy_old_preloads());
			}

			for (unsigned int j = 0; j < _NUM_PLAYHEADS; ++j) {
				_playheads[j].cur_clip_dirty[i] = true;
				_decoders[j][i].invalidate_last_clip_idx();
			}

			_track_dirty[i] = false;
		}
	}

	for (unsigned int i = 0; i < _NUM_PLAYHEADS; ++i) {
		for (unsigned int j = 0; j < _NUM_TRACKS; ++j) {
			if (_playheads[i].cur_clip_dirty[j]) {
				_update_audio_cur_clip_idx(i, j);
				_playheads[i].cur_clip_dirty[j] = false;
			}
		}

		_playheads[i].jumping = false;
	}
}

void IOEngine::insert_clip(unsigned int track, double start, double end,
	unsigned int song_id, ma_uint64 first_frame, double pitch_shift)
{
	IOMsg *msg = _msg_pool->allocate();
	msg->type = IOMsgType::INSERT_CLIP;
	msg->contents.insert_clip.track = track;
	msg->contents.insert_clip.song_id = song_id;
	msg->contents.insert_clip.start = start;
	msg->contents.insert_clip.end = end;
	msg->contents.insert_clip.pitch_shift = pitch_shift;
	msg->contents.insert_clip.first_frame = first_frame;
	_msg_queue.push(msg);
}

void IOEngine::erase_clips_range(unsigned int track, double from, double to)
{
	IOMsg *msg = _msg_pool->allocate();
	msg->type = IOMsgType::ERASE_CLIPS_RANGE;
	msg->contents.erase_clips_range.track = track;
	msg->contents.erase_clips_range.from = from;
	msg->contents.erase_clips_range.to = to;
	_msg_queue.push(msg);
}

void IOEngine::delete_clips(AudioClipsArray clips)
{
	IOMsg *msg = _msg_pool->allocate();
	msg->type = IOMsgType::DELETE_CLIPS;
	msg->contents.delete_clips.clips = clips;
	_msg_queue.push(msg);
}

void IOEngine::delete_old_preloads(OldPreloadsArray old_preloads)
{
	IOMsg *msg = _msg_pool->allocate();
	msg->type = IOMsgType::DELETE_OLD_PRELOADS;
	msg->contents.delete_old_preloads.old_preloads = old_preloads;
	_msg_queue.push(msg);
}

void IOEngine::delete_playhead_chunk(PlayheadChunk *chunk)
{
	IOMsg *msg = _msg_pool->allocate();
	msg->type = IOMsgType::DELETE_PLAYHEAD_CHUNK;
	msg->contents.delete_playhead_chunk.chunk = chunk;
	_msg_queue.push(msg);
}

void IOEngine::jump_playhead(unsigned int playhead, double beat)
{
	IOMsg *msg = _msg_pool->allocate();
	msg->type = IOMsgType::JUMP_PLAYHEAD;
	msg->contents.jump_playhead.playhead = playhead;
	msg->contents.jump_playhead.beat = beat;
	_msg_queue.push(msg);
}

void IOEngine::notify_audio_playhead_jumped(unsigned int playhead)
{
	IOMsg *msg = _msg_pool->allocate();
	msg->type = IOMsgType::AUDIO_PLAYHEAD_JUMPED;
	msg->contents.audio_playhead_jumped.playhead = playhead;
	_msg_queue.push(msg);
}

void IOEngine::_decode_next_cache_chunks(unsigned int playhead_idx,
	unsigned int track_idx)
{
	if (_playheads[playhead_idx].block_decode) {
		return;
	}

	unsigned int clip_idx = _audio->get_playhead_cur_clip_idx(playhead_idx,
		track_idx);
	unsigned int song_id = _audio->get_playhead_cur_song_id(playhead_idx,
		track_idx);

	IOTrack &track = _tracks[track_idx];
	if (!track.is_clip_valid(clip_idx) ||
		!_library->is_song_id_valid(song_id)) {
		return;
	}

	const AudioClip &clip = track.clip_at(clip_idx);

	// If the clip is less than one beat long, don't cache anything (the
	// content should have already been preloaded). We want to avoid the
	// decoder opening and closing lots of files, which would thrash the
	// filesystem.
	// Also, if the playhead is not actually inside the clip, don't cache
	// anything (because it's not actually playing - it's just cued or
	// something). This would need to be fixed later when implementing clip
	// looping.
	double playhead_beat = _audio->get_playhead_beat(playhead_idx);
	if (clip.end - clip.start < 1.0 ||
		playhead_beat < clip.start || playhead_beat >= clip.end) {
		return;
	}

	IOAudioFileDecoder &decoder = _decoders[playhead_idx][track_idx];
	decoder.set_clip_idx(clip_idx, clip);
	decoder.set_song_id(song_id);

	ma_uint64 cur_want_frame = _audio->get_cur_want_frame(playhead_idx,
		track_idx);
	PlayheadChunk *chunk = nullptr;
	while (chunk = decoder.decode(cur_want_frame)) {
		_audio->receive_playhead_chunk(playhead_idx, track_idx, chunk);
	}
}

bool IOEngine::_is_track_valid(unsigned int track_idx)
{
	return track_idx < _NUM_TRACKS;
}

bool IOEngine::_is_playhead_valid(unsigned int playhead_idx)
{
	return playhead_idx < _NUM_PLAYHEADS;
}

void IOEngine::_handle_insert_clip(IOMsgInsertClip &msg)
{
	if (_is_track_valid(msg.track)) {
		_tracks[msg.track].insert_clip(msg.start, msg.end, msg.song_id,
			msg.first_frame, msg.pitch_shift);
		_track_dirty[msg.track] = true;
	}
}

void IOEngine::_handle_erase_clips_range(IOMsgEraseClipsRange &msg)
{
	if (_is_track_valid(msg.track)) {
		_tracks[msg.track].erase_clips_range(msg.from, msg.to);
		_track_dirty[msg.track] = true;
	}
}

void IOEngine::_handle_delete_clips(IOMsgDeleteClips &msg)
{
	if (msg.clips.clips) {
		delete[] msg.clips.clips;
	}
}

void IOEngine::_handle_delete_old_preloads(IOMsgDeleteOldPreloads &msg)
{
	if (msg.old_preloads.frames) {
		for (unsigned int i = 0; i < msg.old_preloads.num_preloads;
			++i) {
			if (msg.old_preloads.frames[i]) {
				delete[] msg.old_preloads.frames[i];
			}
		}

		delete[] msg.old_preloads.frames;
	}
}

void IOEngine::_handle_delete_playhead_chunk(IOMsgDeletePlayheadChunk &msg)
{
	if (msg.chunk) {
		if (msg.chunk->frames) {
			delete[] msg.chunk->frames;
		}

		delete msg.chunk;
	}
}

void IOEngine::_handle_jump_playhead(IOMsgJumpPlayhead &msg)
{
	if (_is_playhead_valid(msg.playhead)) {
		DirtyPlayheadInfo &playhead = _playheads[msg.playhead];
		playhead.jump_beat = msg.beat;
		playhead.jumping = true;
		playhead.block_decode = true;
		for (unsigned int i = 0; i < _NUM_TRACKS; ++i) {
			playhead.cur_clip_dirty[i] = true;
			_decoders[msg.playhead][i].playhead_jumped();
		}
	}
}

void IOEngine::_handle_audio_playhead_jumped(IOMsgAudioPlayheadJumped &msg)
{
	if (_is_playhead_valid(msg.playhead)) {
		_playheads[msg.playhead].block_decode = false;
	}
}

void IOEngine::_update_audio_cur_clip_idx(unsigned int playhead_idx,
	unsigned int track_idx)
{
	DirtyPlayheadInfo &playhead = _playheads[playhead_idx];
	IOTrack &track = _tracks[track_idx];
	unsigned int num_clips = track.num_clips();

	unsigned int cur_clip_idx = 0;

	double playhead_beat = 0.0;
	if (playhead.jumping) {
		playhead_beat = playhead.jump_beat;
	} else if (_audio) {
		playhead_beat = _audio->get_playhead_beat(playhead_idx);
	}

	for (unsigned int i = 0; i < num_clips; ++i) {
		if (playhead_beat >= track.clip_at(i).start) {
			cur_clip_idx = i;
			break;
		}
	}

	if (_audio) {
		if (playhead.jumping) {
			_audio->jump_playhead(playhead_idx, track_idx,
				cur_clip_idx, playhead_beat);
		} else {
			_audio->receive_cur_clip_idx(playhead_idx, track_idx,
				cur_clip_idx);
		}
	}
}
}
