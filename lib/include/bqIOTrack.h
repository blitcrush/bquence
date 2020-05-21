#ifndef BQIOTRACK_H
#define BQIOTRACK_H

#include "bqOldPreloadsArray.h"
#include "bqAudioClipsArray.h"
#include "bqAudioClip.h"
#include "bqAudioClipPreload.h"
#include "bqLibrary.h"
#include "bqConfig.h"

#include <miniaudio.h>

#include <vector>
#include <string>

namespace bq {
class IOTrack {
public:
	IOTrack() {}
	~IOTrack() {}

	void set_preload_config(ma_uint32 num_channels, ma_uint32 sample_rate);

	void bind_library(Library *library);

	void insert_clip(double start, double end, double fade_in,
		double fade_out, unsigned int song_id, ma_uint64 first_frame,
		double pitch_shift);
	void erase_clips_range(double from, double to);

	//
	// After all edit messages have been processed, the contents of
	// copy_clips() and copy_old_preloads() should be sent (in that order)
	// to the AudioEngine
	//
	AudioClipsArray copy_clips();
	OldPreloadsArray copy_old_preloads();

	unsigned int num_clips();
	const AudioClip &clip_at(unsigned int i);

	bool is_clip_valid(unsigned int clip_idx);

	static constexpr ma_uint64 PRELOAD_NUM_FRAMES = PRELOADER_NUM_FRAMES;

private:
	AudioClipPreload _preload(unsigned int song_id, ma_uint64 first_frame);

	std::vector<AudioClip> _clips;
	std::vector<float *> _old_preloads;

	Library *_library = nullptr;

	ma_uint32 _preload_num_channels = 0, _preload_sample_rate = 0;
};
}

#endif
