#ifndef BQIOMSG_H
#define BQIOMSG_H

#include "bqOldPreloadsArray.h"
#include "bqAudioClipsArray.h"
#include "bqPlayheadChunk.h"

namespace bq {
enum IOMsgLink {
	IO_MSG_NEXT_LINK = 0,
	IO_MSG_NUM_LINKS
};

enum class IOMsgType {
	NONE = 0,
	INSERT_CLIP,
	ERASE_CLIPS_RANGE,
	DELETE_CLIPS,
	DELETE_OLD_PRELOADS,
	DELETE_PLAYHEAD_CHUNK,
	JUMP_PLAYHEAD,
	AUDIO_PLAYHEAD_JUMPED
};

struct IOMsgInsertClip {
	unsigned int track;
	unsigned int song_id;
	double start, end;
	double pitch_shift;
	ma_uint64 first_frame;
};

struct IOMsgEraseClipsRange {
	unsigned int track;
	double from, to;
};

struct IOMsgDeleteClips {
	AudioClipsArray clips;
};

struct IOMsgDeleteOldPreloads {
	OldPreloadsArray old_preloads;
};

struct IOMsgDeletePlayheadChunk {
	PlayheadChunk *chunk;
};

struct IOMsgJumpPlayhead {
	unsigned int playhead;
	double beat;
};

struct IOMsgAudioPlayheadJumped {
	unsigned int playhead;
};

union IOMsgContents {
	IOMsgInsertClip insert_clip;
	IOMsgEraseClipsRange erase_clips_range;
	IOMsgDeleteClips delete_clips;
	IOMsgDeleteOldPreloads delete_old_preloads;
	IOMsgDeletePlayheadChunk delete_playhead_chunk;
	IOMsgJumpPlayhead jump_playhead;
	IOMsgAudioPlayheadJumped audio_playhead_jumped;
};

struct IOMsg {
	IOMsgType type;
	IOMsgContents contents;

	IOMsg *links_[IO_MSG_NUM_LINKS];
};
}

#endif
