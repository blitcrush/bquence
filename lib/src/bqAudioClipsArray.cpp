#include "bqAudioClipsArray.h"

namespace bq {
bool AudioClipsArray::is_clip_valid(unsigned int clip_idx)
{
	return clip_idx < num_clips;
}
}
