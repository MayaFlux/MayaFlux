#pragma once

#include "MayaFlux/Core/Engine.hpp"
#include "gtest/gtest.h"

#include "chrono"

namespace MayaFlux::Test {
struct TestConfig {
    static constexpr unsigned int SAMPLE_RATE = 48000;
    static constexpr unsigned int BUFFER_SIZE = 512;
    static constexpr unsigned int NUM_CHANNELS = 2;
    static constexpr unsigned int TEST_DURATION_MS = 100;
};

class AudioTestHelper {
public:
    static std::unique_ptr<Core::Engine> createTestEngine()
    {
        auto engine = std::make_unique<Core::Engine>();
        auto& stream_info = engine->get_stream_info();
        stream_info.sample_rate = TestConfig::SAMPLE_RATE;
        stream_info.buffer_size = TestConfig::BUFFER_SIZE;
        stream_info.output.channels = TestConfig::NUM_CHANNELS;
        stream_info.input.channels = 0;
        engine->Init();
        return engine;
    }

    static void waitForAudio(unsigned int ms = TestConfig::TEST_DURATION_MS)
    {
        auto start = std::chrono::steady_clock::now();
        auto end = start + std::chrono::milliseconds(ms);

        while (std::chrono::steady_clock::now() < end) {
            std::this_thread::yield();
        }
    }
};
}
