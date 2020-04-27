#ifndef BQAUDIOCLIPSARRAY_H
#define BQAUDIOCLIPSARRAY_H

#include "bqAudioClip.h"

namespace bq {
struct AudioClipsArray {
	bool is_clip_valid(unsigned int clip_idx);

	unsigned int num_clips;
	AudioClip *clips;
};
}

#endif
