#include "AudioCallback.hpp"
#include "Engine.hpp"

int MayaFlux::Core::rtaudio_callback(void* output_buffer, void* input_buffer, unsigned int num_frames, double stream_time, RtAudioStreamStatus status, void* user_data)
{
    Engine* engine = static_cast<Engine*>(user_data);

    if (input_buffer && output_buffer) {
        return engine->process_audio(static_cast<double*>(input_buffer), static_cast<double*>(output_buffer), num_frames);
    } else if (output_buffer) {
        return engine->process_output(static_cast<double*>(output_buffer), num_frames);
    } else if (input_buffer) {
        return engine->process_input(static_cast<double*>(input_buffer), num_frames);
    }
    return 0;
}
