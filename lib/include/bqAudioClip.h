#ifndef BQAUDIOCLIP_H
#define BQAUDIOCLIP_H

#include "bqAudioClipPreload.h"

#include <miniaudio.h>

namespace bq {
class AudioClip {
public:
	AudioClip() {}
	~AudioClip() {}

	ma_uint64 pull_preload(float *dest, ma_uint64 first_pull_frame,
		ma_uint64 num_pull_frames);

	double start = -1.0, end = -1.0; // Measured in beats
	double pitch_shift = 0.0; // Measured in semitones
	ma_uint64 first_frame = 0;
	unsigned int song_id = 0;

	AudioClipPreload preload;

private:
	void _copy_frames(float *dest, float *src, ma_uint64 dest_first_frame,
		ma_uint64 src_first_frame, ma_uint64 num_frames,
		ma_uint64 num_channels);
};
}

#endif
