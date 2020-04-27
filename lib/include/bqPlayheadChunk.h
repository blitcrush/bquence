#ifndef BQPLAYHEADCHUNK_H
#define BQPLAYHEADCHUNK_H

#include <miniaudio.h>

namespace bq {
struct PlayheadChunk {
	PlayheadChunk *next;

	ma_uint32 num_channels, sample_rate;
	ma_uint64 first_frame, num_frames;
	float *frames;

	unsigned int song_id;
};
}

#endif
