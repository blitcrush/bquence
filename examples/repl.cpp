#include <miniaudio.h>
#include <bqConfig.h>
#include <bqWorld.h>

#include <iostream>
#include <sstream>
#include <thread>

struct AudioThreadUserData {
	bq::World *world = nullptr;
	float *tmp_frames = nullptr;
	bool track_enabled[4] = { false };
	bool playhead_playing[2] = { false };
	bool running = false;
};

struct IOThreadUserData {
	bq::World *world = nullptr;
	bool track_enabled[4] = { false };
	bool playhead_playing[2] = { false };
	bool running = false;
};

void render_playhead_track(float *out_frames, float *tmp_frames,
	bq::World *world, ma_uint32 num_channels, ma_uint32 channel_stride,
	unsigned int playhead, unsigned int track, float gain,
	ma_uint32 num_frames)
{
	world->pull_audio(playhead, track, tmp_frames, num_frames);

	for (ma_uint32 i = 0; i < num_frames; ++i) {
		ma_uint32 i_adj = i * num_channels + channel_stride;
		out_frames[i_adj] += tmp_frames[i_adj] * gain;
	}
}

void audio_callback(ma_device *device, void *out_frames, const void *in_frames,
	ma_uint32 num_frames)
{
	ma_uint32 num_channels = device->playback.channels;
	float *float_out_frames = static_cast<float *>(out_frames);

	if (!device->pUserData) {
		return;
	}

	AudioThreadUserData *user_data = static_cast<AudioThreadUserData *>(
		device->pUserData);

	if (!user_data->running) {
		return;
	}

	bq::World *world = user_data->world;

	if (!world) {
		return;
	}

	world->pump_audio_thread();

	for (unsigned int i = 0; i < bq::WORLD_NUM_PLAYHEADS; ++i) {
		if (user_data->playhead_playing[i]) {
			for (unsigned int j = 0; j < bq::WORLD_NUM_TRACKS;
				++j) {
				if (user_data->track_enabled[j]) {
					// variable "i" is used for channel
					// stride because playhead 1 should play
					// in left channel and playhead 2 should
					// play in right channel
					render_playhead_track(float_out_frames,
						user_data->tmp_frames, world,
						num_channels, i, i, j, 0.25f,
						num_frames);
				}
			}

			world->pull_done_advance_playhead(i, num_frames);
		}
	}
}

void io_thread_callback(IOThreadUserData *user_data)
{
	while (user_data->running) {
		user_data->world->pump_io_thread();

		for (unsigned int i = 0; i < bq::WORLD_NUM_PLAYHEADS; ++i) {
			if (user_data->playhead_playing[i]) {
				for (unsigned int j = 0;
					j < bq::WORLD_NUM_TRACKS;
					++j) {
					if (user_data->track_enabled[j]) {
						user_data->world->decode_chunks(
							i, j);
					}
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void handle_insert(bq::World *world, std::string cmd_line)
{
	std::istringstream stream(cmd_line);

	std::string cmd_str;
	unsigned int track = 0;
	double start = 0.0, end = 0.0;
	double fade_in = 0.0, fade_out = 0.0;
	double pitch_shift = 0.0;
	ma_uint64 first_frame = 0;
	unsigned int song_id = 0;

	stream >> cmd_str >> track >> start >> end >> fade_in >> fade_out >>
		pitch_shift >> first_frame >> song_id;

	world->insert_clip(track, start, end, fade_in, fade_out, pitch_shift,
		first_frame, song_id);
}

void handle_erase_range(bq::World *world, std::string cmd_line)
{
	std::istringstream stream(cmd_line);

	std::string cmd_str;
	unsigned int track = 0;
	double from = 0.0, to = 0.0;

	stream >> cmd_str >> track >> from >> to;

	world->erase_clips_range(track, from, to);
}

void handle_load_song(bq::World *world, std::string cmd_line)
{
	std::istringstream stream(cmd_line);

	std::string cmd_str;
	std::string filename;
	double sample_rate = 0.0;
	double bpm = 0.0;

	stream >> cmd_str >> filename >> sample_rate >> bpm;

	unsigned int id = world->add_song(filename, sample_rate, bpm);

	std::cout << "Song loaded. ID: " << id << std::endl;
}

void handle_toggle_playhead(AudioThreadUserData &audio_user_data,
	IOThreadUserData &io_user_data, std::string cmd_line)
{
	std::istringstream stream(cmd_line);

	std::string cmd_str;
	unsigned int playhead = 0;

	stream >> cmd_str >> playhead;

	if (playhead < bq::WORLD_NUM_PLAYHEADS) {
		bool &playing = audio_user_data.playhead_playing[playhead];
		playing = !playing;
		io_user_data.playhead_playing[playhead] = playing;
	}
}

void handle_toggle_track(AudioThreadUserData &audio_user_data,
	IOThreadUserData &io_user_data, std::string cmd_line)
{
	std::istringstream stream(cmd_line);

	std::string cmd_str;
	unsigned int track = 0;

	stream >> cmd_str >> track;

	if (track < bq::WORLD_NUM_TRACKS) {
		bool &enabled = audio_user_data.track_enabled[track];
		enabled = !enabled;
		io_user_data.track_enabled[track] = enabled;
	}
}

void handle_jump(bq::World *world, std::string cmd_line)
{
	std::istringstream stream(cmd_line);

	std::string cmd_str;
	unsigned int playhead = 0;
	double beat = 0.0;

	stream >> cmd_str >> playhead >> beat;

	if (playhead < bq::WORLD_NUM_PLAYHEADS) {
		world->set_playhead_beat(playhead, beat);
	}
}

void handle_bpm(bq::World *world, std::string cmd_line)
{
	std::istringstream stream(cmd_line);

	std::string cmd_str;
	double bpm = 0.0;

	stream >> cmd_str >> bpm;

	world->set_bpm(bpm);
}

int main(int argc, char *argv[])
{
	constexpr unsigned int OUTPUT_DEVICE_NUM_CHANNELS = 2;
	constexpr unsigned int OUTPUT_DEVICE_SAMPLE_RATE = 44100;

	AudioThreadUserData audio_user_data;
	IOThreadUserData io_user_data;

	// Since this is just a demo application and we don't know how many
	// frames miniaudio is going to request in the audio callback, simply
	// allocate a huge number here. If you use a framework that allows you
	// to specify the output buffer size, this array would just be as big as
	// (output buffer size * num channels)
	audio_user_data.tmp_frames = new float[
		192000 * OUTPUT_DEVICE_NUM_CHANNELS];

	ma_device_config device_cfg = ma_device_config_init(
		ma_device_type_playback);
	device_cfg.playback.format = ma_format_f32;
	device_cfg.playback.channels = OUTPUT_DEVICE_NUM_CHANNELS;
	device_cfg.sampleRate = OUTPUT_DEVICE_SAMPLE_RATE;
	device_cfg.dataCallback = audio_callback;
	device_cfg.pUserData = &audio_user_data;

	ma_device device;
	if (ma_device_init(nullptr, &device_cfg, &device) != MA_SUCCESS) {
		std::cerr << "Failed to open playback device" << std::endl;
		return -1;
	}

	if (ma_device_start(&device) != MA_SUCCESS) {
		std::cerr << "Failed to start playback device" << std::endl;
		ma_device_uninit(&device);
		return -1;
	}

	bq::World *world = new bq::World(2, OUTPUT_DEVICE_SAMPLE_RATE);
	std::thread io_thread(io_thread_callback, &io_user_data);

	audio_user_data.world = world;
	io_user_data.world = world;
	audio_user_data.running = true;
	io_user_data.running = true;

	std::cout << "Welcome to bquence REPL demo!" << std::endl;
	std::cout << std::endl;
	std::cout << "(The output of the first playhead is routed to the left \
channel, and the output of the second playhead is routed to the right channel)."
<< std::endl;
	std::cout << std::endl;
	std::cout << "Syntax" << std::endl;
	std::cout << "======" << std::endl;
	std::cout << "Load song: l <filename> <sample rate> <BPM>" << std::endl;
	std::cout << "Insert clip: i <track index> <start beat> <end beat> \
<fade in length> <fade out length> <pitch shift> <first frame> <song ID>" <<
std::endl;
	std::cout << "Erase range: e <track index> <from beat> <to beat>" <<
		std::endl;
	std::cout << "Toggle playhead: p <playhead index>" << std::endl;
	std::cout << "Toggle track: t <track index>" << std::endl;
	std::cout << "Jump playhead: j <playhead index> <beat>" << std::endl;
	std::cout << "Set master tempo: b <BPM>" << std::endl;
	std::cout << "Quit: q" << std::endl;
	std::cout << std::endl;
	std::cout << "Valid track indices are 1-4; valid playhead indices are \
1 and 2" << std::endl;
	std::cout << std::endl;
	std::cout << "Have fun!" << std::endl;
	std::cout << std::endl;

	std::string cmd_line;
	bool running = true;

	while (running && std::getline(std::cin, cmd_line)) {
		if (cmd_line.size() > 0) {
			switch (cmd_line.at(0)) {
			case 'i':
				handle_insert(world, cmd_line);
				break;

			case 'e':
				handle_erase_range(world, cmd_line);
				break;

			case 'l':
				handle_load_song(world, cmd_line);
				break;

			case 'p':
				handle_toggle_playhead(audio_user_data,
					io_user_data,
					cmd_line);
				break;

			case 't':
				handle_toggle_track(audio_user_data,
					io_user_data, cmd_line);
				break;

			case 'j':
				handle_jump(world, cmd_line);
				break;

			case 'b':
				handle_bpm(world, cmd_line);
				break;

			case 'q':
				running = false;
				break;

			default:
				break;
			}

			// Append a blank line after each read-eval pair
			std::cout << std::endl;
		} else {
			running = false;
		}
	}

	audio_user_data.running = false;
	io_user_data.running = false;

	ma_device_uninit(&device);
	delete[] audio_user_data.tmp_frames;

	io_thread.join();

	delete world;

	return 0;
}
