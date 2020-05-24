#include "bqWorld.h"

namespace bq {
World::World(ma_uint32 num_channels, ma_uint32 sample_rate)
{
	_library = new Library;
	_library->set_out_sample_rate(sample_rate);

	_audio = new AudioEngine;
	_audio->set_playback_config(num_channels, sample_rate);
	_audio->bind_library(_library);

	_io = new IOEngine;
	_io->set_decode_config(num_channels, sample_rate);
	_io->bind_library(_library);

	_io->bind_audio_engine(_audio);
	_audio->bind_io_engine(_io);
}

World::~World()
{
	delete _audio;
	_audio = nullptr;

	delete _io;
	_io = nullptr;

	delete _library;
	_library = nullptr;
}

double World::get_bpm()
{
	double bpm = 0.0;

	if (_audio) {
		bpm = _audio->get_bpm();
	}

	return bpm;
}

void World::set_bpm(double bpm)
{
	if (_audio) {
		_audio->set_bpm(bpm);
	}
}

double World::get_playhead_beat(unsigned int playhead_idx)
{
	double beat = 0.0;

	if (_audio) {
		beat = _audio->get_playhead_beat(playhead_idx);
	}

	return beat;
}

void World::set_playhead_beat(unsigned int playhead_idx, double beat)
{
	if (_audio) {
		_io->jump_playhead(playhead_idx, beat);
	}
}

unsigned int World::add_song(const std::string &filename, double sample_rate,
	double bpm)
{
	unsigned int song_id = 0;

	if (_library) {
		song_id = _library->add_song(filename, sample_rate, bpm);
	}

	return song_id;
}

void World::insert_clip(unsigned int track_idx, double start_beat,
	double end_beat, double fade_in_beats, double fade_out_beats,
	double pitch_shift_semitones, ma_uint64 first_frame,
	unsigned int song_id)
{
	if (_io) {
		_io->insert_clip(track_idx, start_beat, end_beat, fade_in_beats,
			fade_out_beats, song_id, first_frame,
			pitch_shift_semitones);
	}
}

void World::erase_clips_range(unsigned int track_idx, double from_beat,
	double to_beat)
{
	if (_io) {
		_io->erase_clips_range(track_idx, from_beat, to_beat);
	}
}

void World::pump_audio_thread()
{
	if (_audio) {
		_audio->handle_all_msgs();
	}
}

void World::pump_io_thread()
{
	if (_io) {
		_io->handle_all_msgs();
	}
}

void World::decode_chunks(unsigned int playhead_idx, unsigned int track_idx)
{
	if (_io) {
		_io->decode_next_cache_chunks(playhead_idx, track_idx);
	}
}

void World::pull_audio(unsigned int playhead_idx, unsigned int track_idx,
	float *out_frames, ma_uint64 num_frames)
{
	if (_audio) {
		_audio->pull(playhead_idx, track_idx, out_frames, num_frames);
	}
}

void World::pull_done_advance_playhead(unsigned int playhead_idx,
	ma_uint64 num_frames)
{
	if (_audio) {
		_audio->pull_done_advance_playhead(playhead_idx, num_frames);
	}
}
}
