#ifndef BQAUDIOCLIPPRELOAD_H
#define BQAUDIOCLIPPRELOAD_H

#include <miniaudio.h>

namespace bq {
struct AudioClipPreload {
	ma_uint64 num_channels = 0;
	ma_uint64 sample_rate = 0;
	ma_uint64 first_frame = 0;
	ma_uint64 num_frames = 0;
	float *frames = nullptr;
};
}

#endif
