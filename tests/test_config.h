#include "MayaFlux/Core/Engine.hpp"
#include "gtest/gtest.h"

#include "chrono"

#ifdef RTAUDIO_BACKEND
#include "RtAudio.h"
#endif

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
        engine->Init(TestConfig::SAMPLE_RATE, TestConfig::BUFFER_SIZE, TestConfig::NUM_CHANNELS);
        return engine;
    }

    static void waitForAudio(unsigned int ms = TestConfig::TEST_DURATION_MS)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};
}
