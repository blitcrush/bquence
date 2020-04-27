#include "bqLibrarySongInfo.h"

namespace bq {
LibrarySongInfo::LibrarySongInfo(const std::string &filename,
	double sample_rate, double bpm)
{
	_filename = filename;
	_sample_rate = sample_rate;
	_bpm = bpm;
	_recalc_conversion_factors();
}

void LibrarySongInfo::set_filename(const std::string &filename)
{
	_filename = filename;
}

void LibrarySongInfo::set_sample_rate(double sample_rate)
{
	_sample_rate = sample_rate;
	_recalc_conversion_factors();
}

void LibrarySongInfo::set_bpm(double bpm)
{
	_bpm = bpm;
	_recalc_conversion_factors();
}

const std::string &LibrarySongInfo::get_filename() const
{
	return _filename;
}

double LibrarySongInfo::get_sample_rate() const
{
	return _sample_rate;
}

double LibrarySongInfo::get_bpm() const
{
	return _bpm;
}

ma_uint64 LibrarySongInfo::beats_to_samples(double beats) const
{
	if (beats < 0.0) {
		beats = -beats;
	}

	// + 0.5 is a cheap way to round when we don't have to worry about
	// negative numbers (which we don't, because this is unsigned)
	return static_cast<ma_uint64>(beats * _beats_to_samples + 0.5);
}

double LibrarySongInfo::samples_to_beats(double samples) const
{
	return samples * _samples_to_beats;
}

void LibrarySongInfo::_recalc_conversion_factors()
{
	_beats_to_samples = (60.0 / _bpm) * _sample_rate;
	_samples_to_beats = 1.0 / _beats_to_samples;
}
}
