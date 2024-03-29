#ifndef BQIOAUDIOFILEDECODER_H
#define BQIOAUDIOFILEDECODER_H

#include "bqAudioClip.h"
#include "bqPlayheadChunk.h"
#include "bqLibrary.h"
#include "bqConfig.h"

#include <miniaudio.h>

namespace bq {
class IOAudioFileDecoder {
public:
	IOAudioFileDecoder() {}
	~IOAudioFileDecoder();

	void set_decode_config(ma_uint32 num_channels, ma_uint32 sample_rate);

	void bind_library(Library *library);

	void set_clip_idx(unsigned int clip_idx);
	void set_song_id(unsigned int song_id);
	PlayheadChunk *decode(ma_uint64 from_frame);

	void invalidate_last_clip_idx();
	void reset_next_send_frame();

private:
	void _open_file(unsigned int song_id);
	void _close_file();

	ma_uint32 _num_channels = 0;
	ma_uint32 _sample_rate = 0;

	ma_uint64 _decoder_cur_frame = 0;
	ma_uint64 _last_from_frame = 0;
	ma_uint64 _next_send_frame = 0;
	bool _next_send_frame_valid = false;

	struct _LastClipInfo {
		double start = -1.0;
		ma_uint64 first_frame = 0;
		unsigned int song_id = 0;
	} _last_clip;
	bool _last_clip_valid = false;

	unsigned int _last_clip_idx = 0;
	bool _last_clip_idx_valid = false;

	unsigned int _last_song_id = 0;
	bool _last_song_id_valid = false;

	ma_decoder _decoder;
	bool _decoder_ready = false;

	bool _end_of_song = false;

	static constexpr ma_uint64 _SEND_FRAME_WINDOW =
		STREAMER_NEXT_CHUNK_WINDOW_NUM_FRAMES;
	static constexpr ma_uint64 _CHUNK_NUM_FRAMES =
		STREAMER_CHUNK_NUM_FRAMES;

	Library *_library = nullptr;
};
}

#endif
