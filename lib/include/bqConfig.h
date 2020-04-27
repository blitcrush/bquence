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
}

#endif
