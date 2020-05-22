# bquence

bquence is an audio file clip sequencing engine designed for professional use at [blitcrush](https://blitcrush.com). It's intended to be as small as possible and to fit conveniently inside any C++ project.

## Features

* Minimalist, standalone implementation independent of any DAW, mixer, effects, or audio engine
* Liberally licensed, available under your choice of either Public Domain (Unlicense) or MIT No Attribution
* Multiple playheads (and tracks) simultaneously active in the same session
* Independent time stretching and pitch shifting of individuals clips, synchronized to master tempo
* Safe arrangement editing during playback
* Safe master tempo change during playback
* Loads any audio format supported by [miniaudio](https://github.com/dr-soft/miniaudio)
* Hybrid preload + streaming system buffers the beginnings of audio clips and streams the rest from the disk, enabling low memory consumption while maintaining glitch-free playback under reasonable system conditions

## Getting Started & Documentation

The [bquence wiki](./wiki) contains a [quick-start tutorial](./wiki/quick-start) and [how-to reference/FAQ](./wiki/how-to), along with other more in-depth content. The [quick-start](./wiki/quick-start) is the recommended way for new users to familiarize themselves with bquence's usage, and the [how-to FAQ](./wiki/how-to) is ideal for anyone wondering how to implement a certain functionality with bquence.

## Credits

bquence uses [miniaudio](https://github.com/dr-soft/miniaudio) by David Reid, [QueueWorld](https://github.com/RossBencina/QueueWorld) by Ross Bencina, and [SoundTouch](https://surina.net/soundtouch/) by Olli Parviainen. Please ensure that you abide by their respective license terms as you use bquence.

## License

Your choice of Public Domain (Unlicense) or MIT No Attribution (please read the [LICENSE](LICENSE) file for more information).
