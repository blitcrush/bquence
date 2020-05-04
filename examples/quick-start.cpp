#include <iostream>
#include <thread>

#include <miniaudio.h>
#include <bqWorld.h>

struct AudioUserData {
	bq::World *world = nullptr;
};

struct IOUserData {
	bq::World *world = nullptr;
	bool running = false;
};

void audio_callback(ma_device *device, void *out_frames,
	const void *in_frames, ma_uint32 num_frames)
{
	AudioUserData *user_data = static_cast<AudioUserData *>(
		device->pUserData);
	bq::World *world = user_data->world;

	if (!world) {
		return;
	}

	// Handle all messages in the audio engine's queue
	world->pump_audio_thread();

	// Zero the output buffer
	float *float_frames = static_cast<float *>(out_frames);
	for (ma_uint32 i = 0; i < num_frames * device->playback.channels; ++i) {
		float_frames[i] = 0.0f;
	}

	// Render audio from the first playhead and the first track
	world->pull_audio(0, 0, float_frames, num_frames);

	// Advance the first playhead according to the number of frames that
	// were rendered during this callback.
	world->pull_done_advance_playhead(0, num_frames);
}

void io_callback(IOUserData *user_data)
{
	while (user_data->running && user_data->world) {
		// Handle all messages in the I/O engine's queue, and stream
		// audio data from the disk if needed.
		user_data->world->pump_io_thread(true);
	}
}

int main(int argc, char *argv[])
{
	const ma_uint32 NUM_CHANNELS = 2;
	const ma_uint32 SAMPLE_RATE = 44100;

	AudioUserData audio_user_data;
	IOUserData io_user_data;

	ma_device_config device_cfg = ma_device_config_init(
		ma_device_type_playback);
	device_cfg.playback.format = ma_format_f32;
	device_cfg.playback.channels = NUM_CHANNELS;
	device_cfg.sampleRate = SAMPLE_RATE;
	device_cfg.dataCallback = audio_callback;
	device_cfg.pUserData = &audio_user_data;

	ma_device device;
	if (ma_device_init(nullptr, &device_cfg, &device) != MA_SUCCESS) {
		std::cerr << "Unable to open playback device" << std::endl;
		return 1;
	}

	if (ma_device_start(&device) != MA_SUCCESS) {
		std::cerr << "Unable to start playback device" << std::endl;
		ma_device_uninit(&device);
		return 1;
	}

	// Initialize the current world given the output device's number of
	// channels and sample rate
	bq::World *world = new bq::World(NUM_CHANNELS, SAMPLE_RATE);

	audio_user_data.world = world;
	io_user_data.world = world;
	io_user_data.running = true;

	std::thread io_thread = std::thread(io_callback, &io_user_data);

	// Load two songs with different tempos
	unsigned int song_1_id = world->add_song("fastsong.mp3", 44100.0,
		130.0);
	unsigned int song_2_id = world->add_song("slowsong.mp3", 44100.0,
		100.0);

	// Insert two clips into the arrangement
	world->insert_clip(0, 0.0, 8.0, 1, 0, song_1_id);
	world->insert_clip(0, 8.0, 16.0, -1, 0, song_2_id);

	// Erase part of the sequence between those clips
	world->erase_clips_range(0, 6.0, 10.0);

	// Wait for user input to exit the application
	std::cout << "Press enter to quit" << std::endl;
	std::string in_line;
	std::getline(std::cin, in_line);

	ma_device_uninit(&device);
	io_user_data.running = false;
	io_thread.join();
	delete world;

	return 0;
}
