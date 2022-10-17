#pragma once

extern "C" void audio_amp_enable();
extern "C" void audio_amp_disable();
extern "C" int audio_volume_get();
extern "C" void audio_volume_set(int value);
extern "C" void audio_init(int sample_rate);
extern "C" void audio_submit(short *stereoAudioBuffer, int frameCount);
extern "C" void audio_terminate();
extern "C" void audio_resume();
