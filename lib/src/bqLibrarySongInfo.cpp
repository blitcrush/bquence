#include "bqLibrarySongInfo.h"

namespace bq {
LibrarySongInfo::LibrarySongInfo(const std::string &filename,
	double sample_rate, double out_sample_rate, double bpm)
{
	_filename = filename;
	_sample_rate = sample_rate;
	_out_sample_rate = out_sample_rate;
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

void LibrarySongInfo::set_out_sample_rate(double out_sample_rate)
{
	_out_sample_rate = out_sample_rate;
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

	return static_cast<ma_uint64>(beats * _beats_to_samples + 0.5);
}

double LibrarySongInfo::samples_to_beats(double samples) const
{
	return samples * _samples_to_beats;
}

ma_uint64 LibrarySongInfo::beats_to_out_samples(double beats) const
{
	if (beats < 0.0) {
		beats = -beats;
	}

	return static_cast<ma_uint64>(beats * _beats_to_out_samples + 0.5);
}

double LibrarySongInfo::out_samples_to_beats(double out_samples) const
{
	return out_samples * _out_samples_to_beats;
}

ma_uint64 LibrarySongInfo::samples_self2out(ma_uint64 self_samples) const
{
	return static_cast<ma_uint64>(static_cast<double>(self_samples) *
		_samples_self2out_factor + 0.5);
}

ma_uint64 LibrarySongInfo::samples_out2self(ma_uint64 out_samples) const
{
	return static_cast<ma_uint64>(static_cast<double>(out_samples) *
		_samples_out2self_factor + 0.5);
}

void LibrarySongInfo::_recalc_conversion_factors()
{
	_samples_self2out_factor = _out_sample_rate / _sample_rate;
	_samples_out2self_factor = 1.0 / _samples_self2out_factor;

	_beats_to_samples = (60.0 / _bpm) * _sample_rate;
	_samples_to_beats = 1.0 / _beats_to_samples;

	_beats_to_out_samples = (60.0 / _bpm) * _out_sample_rate;
	_out_samples_to_beats = 1.0 / _beats_to_out_samples;
}
}
