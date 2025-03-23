#pragma once
#include "config.h"

namespace MayaFlux::Core {

int rtaudio_callback(void* output_buffer, void* input_buffer, unsigned int num_frames, double stream_time, RtAudioStreamStatus status, void* user_data);
}
