#ifndef BQCONFIG_H
#define BQCONFIG_H

namespace bq {
constexpr unsigned int WORLD_NUM_TRACKS = 4;
constexpr unsigned int WORLD_NUM_PLAYHEADS = 2;

constexpr unsigned int PRELOADER_NUM_FRAMES = 176400;
constexpr unsigned int STREAMER_CHUNK_NUM_FRAMES = 176400;
constexpr unsigned int STREAMER_NEXT_CHUNK_WINDOW_NUM_FRAMES = 88200;

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
