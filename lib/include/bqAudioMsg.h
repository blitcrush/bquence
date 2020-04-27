#ifndef BQAUDIOMSG_H
#define BQAUDIOMSG_H

#include "bqOldPreloadsArray.h"
#include "bqAudioClipsArray.h"
#include "bqPlayheadChunk.h"

namespace bq {
enum AudioMsgLink {
	AUDIO_MSG_NEXT_LINK = 0,
	AUDIO_MSG_NUM_LINKS
};

enum class AudioMsgType {
	NONE = 0,
	RECEIVE_CLIPS,
	RECEIVE_OLD_PRELOADS,
	RECEIVE_CUR_CLIP_IDX,
	JUMP_PLAYHEAD,
	RECEIVE_PLAYHEAD_CHUNK
};

struct AudioMsgReceiveClips {
	unsigned int track;
	AudioClipsArray clips;
};

struct AudioMsgReceiveOldPreloads {
	OldPreloadsArray old_preloads;
};

struct AudioMsgReceiveCurClipIdx {
	unsigned int playhead;
	unsigned int track;
	unsigned int cur_clip_idx;
};

struct AudioMsgJumpPlayhead {
	unsigned int playhead;
	unsigned int track;
	unsigned int cur_clip_idx;
	double beat;
};

struct AudioMsgReceivePlayheadChunk {
	unsigned int playhead;
	unsigned int track;
	PlayheadChunk *chunk;
};

union AudioMsgContents {
	AudioMsgReceiveClips receive_clips;
	AudioMsgReceiveOldPreloads receive_old_preloads;
	AudioMsgReceiveCurClipIdx receive_cur_clip_idx;
	AudioMsgJumpPlayhead jump_playhead;
	AudioMsgReceivePlayheadChunk receive_playhead_chunk;
};

struct AudioMsg {
	AudioMsgType type;
	AudioMsgContents contents;

	AudioMsg *links_[AUDIO_MSG_NUM_LINKS];
};
}

#endif
