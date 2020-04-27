#ifndef BQLibrarySongInfo_H
#define BQLibrarySongInfo_H

#include <string>

#include <miniaudio.h>

namespace bq {
class LibrarySongInfo {
public:
	LibrarySongInfo() {}
	LibrarySongInfo(const std::string &filename, double sample_rate,
		double bpm);
	~LibrarySongInfo() {}

	void set_filename(const std::string &filename);
	void set_sample_rate(double sample_rate);
	void set_bpm(double bpm);

	const std::string &get_filename() const;
	double get_sample_rate() const;
	double get_bpm() const;

	ma_uint64 beats_to_samples(double beats) const;
	double samples_to_beats(double samples) const;

private:
	void _recalc_conversion_factors();

	std::string _filename;
	double _sample_rate = 0.0;
	double _bpm = 0.0;

	double _beats_to_samples = 0.0, _samples_to_beats = 0.0;
};
}

#endif
