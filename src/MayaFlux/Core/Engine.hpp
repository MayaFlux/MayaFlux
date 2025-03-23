#pragma once

#include "MayaFlux/MayaFlux.hpp"

#include "Device.hpp"

#include "Stream.hpp"

namespace MayaFlux::Core {

using AudioProcessingFunction = std::function<void(double*, double*, unsigned int)>;

class Engine {
public:
    Engine();
    ~Engine();

    void Init(GlobalStreamInfo stream_info);

    inline const std::shared_ptr<Stream> get_stream_settings() const
    {
        return m_StreamSettings;
    }

    void Start();
    void End();

    int process_input(double* input_buffer, unsigned int num_frames);
    int process_output(double* output_buffer, unsigned int num_frames);
    int process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames);

    void add_processor(AudioProcessingFunction processor);
    void clear_processors();

    void process_buffer(std::vector<double>& buffer, unsigned int num_frames);

private:
    std::unique_ptr<RtAudio> m_Context;
    std::shared_ptr<Device> m_Device;
    std::shared_ptr<Stream> m_StreamSettings;

    std::vector<AudioProcessingFunction> m_Processing_chain;

    void execute_processing_chain(double* input_buffer, double* output_buffer, unsigned int num_frames);

    RtAudio* get_handle()
    {
        return m_Context.get();
    }
};
}
