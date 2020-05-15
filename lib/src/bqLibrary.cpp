#include "bqLibrary.h"

namespace bq {
void Library::set_out_sample_rate(double out_sample_rate)
{
	_out_sample_rate = out_sample_rate;
}

unsigned int Library::add_song(const std::string &filename, double sample_rate,
	double bpm)
{
	_songs.push_back(LibrarySongInfo(filename, sample_rate,
		_out_sample_rate, bpm));
	return static_cast<unsigned int>(_songs.size()) - 1;
}

double Library::sample_rate(unsigned int song_id) const
{
	if (is_song_id_valid(song_id)) {
		return _songs[song_id].get_sample_rate();
	} else {
		return 0.0;
	}
}

double Library::bpm(unsigned int song_id) const
{
	if (is_song_id_valid(song_id)) {
		return _songs[song_id].get_bpm();
	} else {
		return 0.0;
	}
}

ma_uint64 Library::beats_to_samples(unsigned int song_id, double beats) const
{
	if (is_song_id_valid(song_id)) {
		return _songs[song_id].beats_to_samples(beats);
	} else {
		return 0;
	}
}

double Library::samples_to_beats(unsigned int song_id, double samples) const
{
	if (is_song_id_valid(song_id)) {
		return _songs[song_id].samples_to_beats(samples);
	} else {
		return 0.0;
	}
}

ma_uint64 Library::beats_to_out_samples(unsigned int song_id, double beats)
const
{
	if (is_song_id_valid(song_id)) {
		return _songs[song_id].beats_to_out_samples(beats);
	} else {
		return 0;
	}
}

double Library::out_samples_to_beats(unsigned int song_id, double out_samples)
const
{
	if (is_song_id_valid(song_id)) {
		return _songs[song_id].out_samples_to_beats(out_samples);
	} else {
		return 0.0;
	}
}

ma_uint64 Library::samples_self2out(unsigned int song_id,
	ma_uint64 self_samples) const
{
	if (is_song_id_valid(song_id)) {
		return _songs[song_id].samples_self2out(self_samples);
	} else {
		return 0;
	}
}

ma_uint64 Library::samples_out2self(unsigned int song_id, ma_uint64 out_samples)
const
{
	if (is_song_id_valid(song_id)) {
		return _songs[song_id].samples_out2self(out_samples);
	} else {
		return 0;
	}
}

const std::string Library::filename(unsigned int song_id) const
{
	if (is_song_id_valid(song_id)) {
		return _songs[song_id].get_filename();
	} else {
		return "";
	}
}

bool Library::is_song_id_valid(unsigned int song_id) const
{
	return song_id < _songs.size();
}
}
