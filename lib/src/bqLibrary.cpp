#include "bqLibrary.h"

namespace bq {
unsigned int Library::add_song(const std::string &filename, double sample_rate,
	double bpm)
{
	_songs.push_back(LibrarySongInfo(filename, sample_rate, bpm));
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
