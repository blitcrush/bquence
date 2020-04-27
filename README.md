# bquence
bquence is an audio file clip sequencing engine designed for professional use at blitcrush. It's intended to be as minimal as possible, and it fits conveniently into any C++ project. Nevertheless, some unusual trade-offs between flexibility and ease of implementation have left their mark on the bquence project, since it was first and foremost designed for the unique needs of blitcrush. (Perhaps the most notable example of this situation: multiple playheads are allowed in a single session, but the maximum number of tracks and playheads in a session must be fixed at compile-time).

## Features
* Minimalist, standalone implementation independent of any DAW, mixer, effects, or audio engine
* Liberally licensed, available under your choice of either Public Domain (Unlicense) or MIT No Attribution
* Multiple playheads (and tracks) simultaneously active in the same session
* Independent time stretching and pitch shifting of individuals clips, synchronized to master tempo
* Safe arrangement editing during playback
* Safe master tempo change during playback
* Hybrid preload + streaming system buffers the beginnings of audio clips and streams the rest from the disk, enabling low memory consumption while maintaining glitch-free playback under reasonable system conditions

## Pre-Alpha Warning
At the moment, bquence is unstable, pre-alpha software. A thorough testing and proper release is estimated to land in slightly over a month. If you see yourself as a potential user of this library, please watch this repository. Meanwhile, feel free to test this codebase and report issues it that's convenient for you. (Please be aware that any feature requests and enhancements will be put on hold until the current features have been stabilized).

## Setup
As with most C++ libraries, the hardest part of using bquence is setting up its dependencies. Thankfully, [miniaudio](https://github.com/dr-soft/miniaudio) and [QueueWorld](https://github.com/RossBencina/QueueWorld) were made to be easily be included into your project in source form. bquence uses [SoundTouch](https://www.surina.net/soundtouch/)'s ```SoundTouchDLL``` interface for dynamic linking, so you'll need to either get that through your package manager (if you're using Visual Studio you can install it via [vcpkg](https://github.com/Microsoft/vcpkg)) or set it up yourself. It's possible that using the DLL wrapper on Linux may require some extra work, as it seems it was originally designed for Windows, but hopefully bquence will include a workaround if necessary by its first proper release.

Once you've set up dependencies, it's probably easiest to compile bquence into your project in source form rather than linking it as a separate library. Simply add ```lib/include``` to your include path and ```lib/src``` to your list of source files, and you're good to go!

## Usage
*To celebrate the first full release of bquence, a code-only example will soon be provided in addition to the slightly verbose explanatory documentation here.*

One convenient header file provides easy access to all the greatness of bquence.

```
#include "bqWorld.h"
```

A ```bq::World``` is roughly equivalent to a sequencing session in a modern DAW - an audio file library, audio engine, and IO engine all bundled into one interface. When instantiating a ```World```, you'll need to tell it how many channels and what sample rate to use for streaming and processing audio. It's probably best to use two channels and a rate of either 44,100 or 48,000 samples per second (depending on what the soundcard prefers). Here's an example:

```
bq::World *world = new bq::World(2, 44100);
```

It's impossible to change the channel count or sample rate of a world; instead, you'd have to delete the existing world and start again. For example:

```
delete world;
world = new bq::World(2, 48000);
```

A world by itself produces audio samples but doesn't actually play them back through your speakers or audio interface - for that, you'll need the help of an external library such as [miniaudio](https://github.com/dr-soft/miniaudio), [PortAudio](http://www.portaudio.com/), or [RtAudio](https://www.music.mcgill.ca/~gary/rtaudio/). Once you have acquired one or more output device(s) and audio callback(s) from your choice of audio I/O library, you can start playing the output of your world into that device. For example:

```
void audio_callback(float *out_frames, unsigned int num_out_frames) {
  // You'll probably want to get the "world" pointer through whatever user data
  // mechanism your audio I/O library provides.
  world->pump_audio_thread();
  world->pull_audio(0, 0, out_frames, num_out_frames);
}
```

```pump_audio_thread()``` must be called at the beginning of each audio callback. It handles messages from the IO thread, such as receiving new audio samples streamed from the disk and responding to arrangement edits.

```pull_audio``` renders interleaved PCM frames into an output buffer for a given playhead and track. It's signature looks like this:

```
void pull_audio(unsigned int playhead_idx, unsigned int track_idx,
  float *out_frames, ma_uint64 num_frames);
```

So the above audio callback example renders the requested ```num_out_frames``` of the output of the first playhead and the first track into the ```out_frames``` buffer. Speaking of playheads and tracks, please don't miss the **Configuration** section below, where you'll learn how to set the number of available playheads and tracks in a world at compile-time.

If you want to implement a mixing system (perhaps applying some gain and effects to certain tracks and routing different playheads to different outputs), you can call ```pull_audio``` with a temporary output buffer, which can then be accessed by your mixing system. At that point, the mixing system can do whatever is appropriate for the current post-sequencer audio, applying effects and making sure it's routed to the correct outputs.

Since audio callbacks are bound by real-time constraints (meaning no memory allocation, locks, or disks I/O operations are advisable inside an audio callback), many important tasks need to take place in a separate thread. bquence calls this thread the I/O thread, and you'll have to create and manage it yourself. Inside that thread, you need to repeatedly call ```pump_io_thread```. ```pump_io_thread``` accepts a boolean argument indicating whether it should sleep (```true```) or whether you'll manage the thread's sleeping yourself (```false```). Normally, if that thread is completely dedicated to bquence, you'll just want to call ```pump_io_thread(true)``` and nothing else. Alternatively, if bquence needs to share that thread with some other code, it's probably best to call ```pump_io_thread(false)``` and schedule that thread yourself. (Do note that you'll want to call ```pump_io_thread``` fairly often, maybe about every 10ms or so, to ensure that it's able to stream data from the disk and send it to the audio engine before that data is immediately needed).

Admittedly, this is complicated at first, but a successful cooperation between the audio and IO threads is essential to the performant functionality of bquence, so it's important to understand the above material and use the provided threading functions wisely.

Once your threads are set up correctly, you can start editing track arrangements, seeking with playheads, and synchronizing to a master tempo. All this functionality is much easier than the prior setup that was required to enable it, and all of it is encapsulated by these fairly self-explanatory functions provided by the ```bq::World``` class. Additionally, all of these functions are safe to call from the main thread.

* ```get_bpm``` and ```set_bpm``` get and set the master tempo of the world, respectively.
* ```get_playhead_beat``` and ```set_playhead_beat``` query and seek the given playhead, respectively.
* ```add_song``` adds a source audio file's metadata to the world's database (known as its "library"). Pass it the filename, sample rate, and BPM of the source file, and it will return an ```unsigned int``` ID usable only inside that world. You'll want to hold onto the ID later, as you'll pass it into ```insert_clip``` whenever you insert a clip referencing that source file into the arrangement.
* ```insert_clip``` inserts an audio clip into a track's arrangement. It takes the following arguments: track index, start and end beats of the clip on the arrangement timeline, pitch shift value (specified in semitones), PCM frame index (PCM frames are often synonymously called "samples" in this context) of the source audio file from which the clip should begin playing, and the song ID described earlier.
* ```erase_clips_range``` erases all clip content from a track between the specified beats. If necessary, it will cut "holes" in clips and/or adjust their start and end points. It takes the following arguments: track index, first beat of range, last beat of range.

Refer to [bqWorld.h](lib/include/bqWorld.h) for function signatures and a complete API reference.

## Configuration
bquence needs to know about certain settings at the time it is compiled. These settings can be adjusted to your preferences by modifying [bqConfig.h](lib/include/bqConfig.h). The two most important are probably ```WORLD_NUM_TRACKS```, which sets the number of tracks that every ```bq::World``` contains, and ```WORLD_NUM_PLAYHEADS```, which sets the number of playheads that every ```bq::World``` contains. (At some point, if this feature is in high demand, these settings may become adjustable on a per-world basis at runtime).

If you're struggling with random audio dropouts, first determine if it's a disk I/O issue (check your threading setup to make sure your I/O thread is not consumed with too much other work or spending too long sleeping, and make sure you're not applying expensive effects or using complex time-stretch settings). If you conclude that it is, in fact, a disk I/O issue, you might want to try increasing ```STREAMER_NEXT_CHUNK_WINDOW_NUM_FRAMES``` up to a few times its current value. That constant effectively controls the minimum number of samples that the I/O thread must stream ahead of audio playback. If that doesn't remedy the issue, try increasing ```STREAMER_CHUNK_NUM_FRAMES```, which specifies the number of frames that the I/O thread sends to the audio thread on each streaming occurrence.

Bear in mind that extremely short periods of silence are normal during a playhead seek, as it's impossible to keep long arrangements entirely in memory and also impossible to predict with perfect accuracy where a user is going to seek. Increasing the values of the constants listed above will not solve this problem and will most likely worsen the situation, as that could stress the I/O thread with too much decoding work and it wouldn't be able to send chunks to the audio thread in time for playback.

## Credits
bquence uses [miniaudio](https://github.com/dr-soft/miniaudio) by David Reid, [QueueWorld](https://github.com/RossBencina/QueueWorld) by Ross Bencina, and [SoundTouch](https://www.surina.net/soundtouch/) by Olli Parviainen. Please ensure that you abide by their respective license terms as you use bquence.

## License
Your choice of Public Domain (Unlicense) or MIT No Attribution (please read the [LICENSE](LICENSE) file for more information).
