#pragma once

#include "config.h"

namespace MayaFlux::Buffers {

class BufferProcessor;
class BufferProcessingChain;

class AudioBuffer : public std::enable_shared_from_this<AudioBuffer> {
public:
    virtual ~AudioBuffer() = default;
    virtual void setup(u_int32_t channel, u_int32_t num_samples) = 0;

    virtual void resize(u_int32_t num_samples) = 0;
    virtual void clear() = 0;
    virtual u_int32_t get_num_samples() const = 0;

    virtual std::vector<double>& get_data() = 0;
    virtual const std::vector<double>& get_data() const = 0;

    virtual void process_default() = 0;
    // virtual void process(const std::any& params) = 0;

    virtual u_int32_t get_channel_id() const = 0;
    virtual void set_channel_id(u_int32_t id) = 0;
    virtual void set_num_samples(u_int32_t num_samples) = 0;

    virtual void set_default_processor(std::shared_ptr<BufferProcessor> processor) = 0;
    virtual std::shared_ptr<BufferProcessor> get_default_processor() const = 0;

    virtual std::shared_ptr<BufferProcessingChain> get_processing_chain() = 0;
    virtual void set_processing_chain(std::shared_ptr<BufferProcessingChain> chain) = 0;
    virtual double& get_sample(u_int32_t index) = 0;
};

class StandardAudioBuffer : public AudioBuffer {
public:
    StandardAudioBuffer();
    StandardAudioBuffer(u_int32_t channel_id, u_int32_t num_samples = 512);
    ~StandardAudioBuffer() override = default;

    void setup(u_int32_t channel, u_int32_t num_samples) override;
    void resize(u_int32_t num_samples) override;
    void clear() override;

    u_int32_t get_num_samples() const override { return m_num_samples; }
    u_int32_t get_channel_id() const override { return m_channel_id; }
    void set_channel_id(u_int32_t id) override { m_channel_id = id; }
    void set_num_samples(u_int32_t num_samples) override;

    std::vector<double>& get_data() override { return m_data; }
    const std::vector<double>& get_data() const override { return m_data; }

    void process_default() override;
    // void process(const std::any& params) override;

    void set_default_processor(std::shared_ptr<BufferProcessor> processor) override;
    std::shared_ptr<BufferProcessor> get_default_processor() const override { return m_default_processor; }

    std::shared_ptr<BufferProcessingChain> get_processing_chain() override { return m_processing_chain; }
    void set_processing_chain(std::shared_ptr<BufferProcessingChain> chain) override;

    inline virtual double& get_sample(u_int32_t index) override { return get_data()[index]; }

protected:
    u_int32_t m_channel_id;
    u_int32_t m_num_samples;
    std::vector<double> m_data;
    u_int32_t m_sample_rate = 48000;

    std::shared_ptr<BufferProcessor> m_default_processor;
    std::shared_ptr<BufferProcessingChain> m_processing_chain;

    virtual std::shared_ptr<BufferProcessor> create_default_processor() { return nullptr; }
};
}
