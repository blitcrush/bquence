#ifndef BQCONFIG_H
#define BQCONFIG_H

namespace bq {
constexpr unsigned int WORLD_NUM_TRACKS = 4;
constexpr unsigned int WORLD_NUM_PLAYHEADS = 2;

//
// These are (hopefully) sensible defaults for applications outputting at a
// sample rate of 44,100. They must be increased proportionally if you plan to
// output at larger rates.
//
//
// Number of frames to pre-load at the beginning of a clip, available for
// immediate usage by the AudioEngine without waiting for the IOEngine to decode
// anything
constexpr unsigned int PRELOADER_NUM_FRAMES = 176400;
// Number of frames to decode and push to the AudioEngine each time a decode is
// requested
//
// Imagine this as the grey line underneath the red line on the YouTube website
// video player.
constexpr unsigned int STREAMER_CHUNK_NUM_FRAMES = 176400;
// When a playhead is closer than this number of frames to the beginning of the
// next chunk that needs to be this number of frames, it decodes it anyway even
// though it won't be needed until this number of frames later than the time the
// decode was performed.
//
// Imagine this as the value that determines how close the red line may approach
// to the end of the grey line, before the grey line is extended, on the YouTube
// website video player.
constexpr unsigned int STREAMER_NEXT_CHUNK_WINDOW_NUM_FRAMES = 176400;
//

constexpr unsigned int IO_THREAD_SLEEP_INTERVAL_MILLIS = 10;

constexpr unsigned int ENGINE_MAX_NUM_POOL_MSGS = 1024;

//
// Please refer to sections 3.4 and 3.5 of the SoundTouch readme for more
// information about these parameters:
// https://surina.net/soundtouch/README.html
//
constexpr bool TIMESTRETCH_USE_QUICKSEEK = true;
constexpr int TIMESTRETCH_SEQUENCE_MS = -1; // -1 = Auto-detect based on tempo
constexpr int TIMESTRETCH_SEEKWINDOW_MS = -1; // -1 = Auto-detect based on tempo
constexpr int TIMESTRETCH_OVERLAP_MS = -1; // -1 = Auto-detect based on tempo
}

#endif
