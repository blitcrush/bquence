#ifndef BQWORLD_H
#define BQWORLD_H

#include "bqAudioEngine.h"
#include "bqIOEngine.h"
#include "bqLibrary.h"
#include "bqConfig.h"

#include <miniaudio.h>

#include <thread>

namespace bq {
class World {
public:
	World(ma_uint32 num_channels, ma_uint32 sample_rate);
	~World();

	double get_bpm();
	void set_bpm(double bpm);

	double get_playhead_beat(unsigned int playhead_idx);
	void set_playhead_beat(unsigned int playhead_idx, double beat);

	unsigned int add_song(const std::string &filename, double sample_rate,
		double bpm);

	void insert_clip(unsigned int track_idx, double start_beat,
		double end_beat, double fade_in_beats, double fade_out_beats,
		double pitch_shift_semitones, ma_uint64 first_frame,
		unsigned int song_id);
	void erase_clips_range(unsigned int track_idx, double from_beat,
		double to_beat);

	// Should only be called from the audio thread and should be called at
	// the beginning of every audio callback invocation as long as an audio
	// output is alive
	void pump_audio_thread();
	// SHould only be called from the IO thread and should be called at the
	// beginning of every IO thread callback invocation as long as the
	// engine needs to be alive
	void pump_io_thread(bool should_sleep);

	// Should only be called from the audio thread
	void pull_audio(unsigned int playhead_idx, unsigned int track_idx,
		float *out_frames, ma_uint64 num_frames);
	void pull_done_advance_playhead(unsigned int playhead_idx,
		ma_uint64 num_frames);

private:
	AudioEngine *_audio = nullptr;
	IOEngine *_io = nullptr;
	Library *_library = nullptr;
};
}

#endif
