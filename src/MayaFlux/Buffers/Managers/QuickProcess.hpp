#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Buffers/BufferSpec.hpp"

namespace MayaFlux::Buffers {

template <typename FuncType>
class QuickProcess : public BufferProcessor {
public:
    QuickProcess(FuncType function)
        : m_function(std::move(function))
    {
    }

    void processing_function(std::shared_ptr<Buffer> buffer) override
    {
        if constexpr (std::is_same_v<FuncType, AudioProcessingFunction>) {
            if (auto audio_buf = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
                m_function(audio_buf);
            }
        } else {
            if (auto vk_buf = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
                m_function(vk_buf);
            }
        }
    }

    void on_attach(const std::shared_ptr<Buffer>& /*buffer*/) override
    {
        if constexpr (std::is_same_v<FuncType, AudioProcessingFunction>) {
            m_processing_token = ProcessingToken::AUDIO_BACKEND;
        } else {
            m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
        }
    }

    [[nodiscard]] bool is_compatible_with(std::shared_ptr<Buffer> buffer) const override
    {
        if constexpr (std::is_same_v<FuncType, AudioProcessingFunction>) {
            return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
        } else {
            return std::dynamic_pointer_cast<VKBuffer>(buffer) != nullptr;
        }
    }

private:
    FuncType m_function;
};

}
