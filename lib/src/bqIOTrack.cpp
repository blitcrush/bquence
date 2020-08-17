#include "bqIOTrack.h"

namespace bq {
void IOTrack::set_preload_config(ma_uint32 num_channels, ma_uint32 sample_rate)
{
	_preload_num_channels = num_channels;
	_preload_sample_rate = sample_rate;
}

void IOTrack::bind_library(Library *library)
{
	_library = library;
}

void IOTrack::insert_clip(double start, double end, double fade_in,
	double fade_out, unsigned int song_id, ma_uint64 first_frame,
	double pitch_shift)
{
	erase_clips_range(start, end);

	auto it = _clips.begin();

	while (it != _clips.end()) {
		if (it->start >= end) {
			break;
		}

		++it;
	}

	AudioClip clip;
	clip.song_id = song_id;
	clip.start = start;
	clip.end = end;
	clip.fade_in = fade_in;
	clip.fade_out = fade_out;
	clip.pitch_shift = pitch_shift;
	clip.first_frame = _library->samples_self2out(clip.song_id,
		first_frame);
	if (_library) {
		clip.preload = _preload(clip.song_id, clip.first_frame);
	}
	it = _clips.insert(it, clip);
}

void IOTrack::erase_clips_range(double from, double to)
{
	std::vector<AudioClip>::iterator it = _clips.begin();

	while (it != _clips.end()) {
		if (it->end <= from) {
			++it;
			continue;
		}

		if (it->start >= to) {
			break;
		}

		bool contains_start = false, contains_end = false;

		if (from <= it->start) {
			contains_start = true;
		}

		if (to >= it->end) {
			contains_end = true;
		}

		if (contains_start && contains_end) {
			_old_preloads.push_back(it->preload.frames);
			it = _clips.erase(it);
		} else if (contains_start) { // <- PRELOADS HERE
			if (_library) {
				it->first_frame +=
					_library->beats_to_out_samples(
						it->song_id, to - it->start);
				_old_preloads.push_back(it->preload.frames);
				it->preload = _preload(it->song_id,
					it->first_frame);
			}
			it->start = to;

			++it;
		} else if (contains_end) {
			it->end = from;
			++it;
		} else { // <- PRELOADS HERE
			// In this case, the clip contains the range, so we're
			// going to cut a "hole" in the middle of the clip
			// between the beats specified by the arguments "from"
			// and "to"

			AudioClip old = *it;
			// The clip on the left side of the hole will use the
			// same preload frames as this old clip, so we don't
			// push those onto _needs_delete
			it = _clips.erase(it);

			AudioClip first = old;
			first.end = from;
			it = _clips.insert(it, first);

			AudioClip last = old;
			if (_library) {
				last.first_frame +=
					_library->beats_to_out_samples(
						last.song_id, to - last.start);
				last.preload = _preload(last.song_id,
					last.first_frame);
			}
			last.start = to;
			it = _clips.insert(std::next(it), last);

			break;
		}
	}
}

AudioClipsArray IOTrack::copy_clips()
{
	AudioClipsArray result;

	result.num_clips = static_cast<unsigned int>(_clips.size());

	if (result.num_clips > 0) {
		result.clips = new AudioClip[result.num_clips];
		std::copy(_clips.begin(), _clips.end(), result.clips);
	} else {
		result.clips = nullptr;
	}

	return result;
}

OldPreloadsArray IOTrack::copy_old_preloads()
{
	OldPreloadsArray result;

	result.num_preloads = static_cast<unsigned int>(_old_preloads.size());

	if (result.num_preloads > 0) {
		result.frames = new float *[result.num_preloads];
		std::copy(_old_preloads.begin(), _old_preloads.end(),
			result.frames);
		_old_preloads.clear();
	} else {
		result.frames = nullptr;
	}

	return result;
}

unsigned int IOTrack::num_clips()
{
	return static_cast<unsigned int>(_clips.size());
}

const AudioClip &IOTrack::clip_at(unsigned int i)
{
	return _clips[i];
}

bool IOTrack::is_clip_valid(unsigned int clip_idx)
{
	return clip_idx < _clips.size();
}

AudioClipPreload IOTrack::_preload(unsigned int song_id, ma_uint64 first_frame)
{
	AudioClipPreload result;

	ma_decoder_config decoder_cfg = ma_decoder_config_init(ma_format_f32,
		_preload_num_channels, _preload_sample_rate);

	ma_decoder decoder;
	if (ma_decoder_init_file(_library->filename(song_id).c_str(),
		&decoder_cfg, &decoder) == MA_SUCCESS) {
		if (ma_decoder_seek_to_pcm_frame(&decoder, first_frame) ==
			MA_SUCCESS) {
			result.num_channels = _preload_num_channels;
			result.sample_rate = _preload_sample_rate;
			result.first_frame = first_frame;
			result.frames = new float[PRELOAD_NUM_FRAMES *
				_preload_num_channels];
			result.num_frames = ma_decoder_read_pcm_frames(&decoder,
				result.frames, PRELOAD_NUM_FRAMES);
		}

		ma_decoder_uninit(&decoder);
	}

	return result;
}
}
