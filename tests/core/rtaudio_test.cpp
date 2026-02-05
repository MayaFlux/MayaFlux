#include "../test_config.h"

#include "MayaFlux/Core/Backends/Audio/RtAudioBackend.hpp"
#include "MayaFlux/Core/Backends/Audio/RtAudioSingleton.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/Subsystems/AudioSubsystem.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"

#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "RtAudio.h"

// Define INTEGRATION_TEST to enable tests that interact with real audio devices
// Comment out for CI environments without audio hardware
#define INTEGRATION_TEST

// Define AUDIBLE_TEST to run tests that produce audible output with longer durations
// This is useful for manual testing of audio functionality
#define AUDIBLE_TEST

namespace MayaFlux::Test {

#ifdef INTEGRATION_TEST

//-------------------------------------------------------------------------
// Audio Backend and Device Discovery Tests
//-------------------------------------------------------------------------

class AudioBackendTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        backend = std::make_unique<Core::RtAudioBackend>();
        device_manager = backend->create_device_manager();
    }

    void TearDown() override
    {
        device_manager.reset();
        backend.reset();
    }

    std::unique_ptr<Core::IAudioBackend> backend;
    std::unique_ptr<Core::AudioDevice> device_manager;
};

TEST_F(AudioBackendTest, BackendInitialization)
{
    EXPECT_NE(backend, nullptr);
    EXPECT_NE(device_manager, nullptr);

    std::string version = backend->get_version_string();
    EXPECT_FALSE(version.empty()) << "Backend should provide version information";

    int api_type = backend->get_api_type();
    EXPECT_NE(api_type, RtAudio::Api::UNSPECIFIED) << "Backend should use valid API";

    std::cout << "RtAudio Backend Version: " << version << std::endl;
    std::cout << "Active API Type: " << api_type << std::endl;
}

TEST_F(AudioBackendTest, DeviceEnumeration)
{
    const auto& output_devices = device_manager->get_output_devices();
    const auto& input_devices = device_manager->get_input_devices();

    std::cout << "Found " << output_devices.size() << " output devices" << std::endl;
    std::cout << "Found " << input_devices.size() << " input devices" << std::endl;

    EXPECT_GT(output_devices.size(), 0) << "Should find at least one output device";

    for (const auto& device : output_devices) {
        EXPECT_GT(device.output_channels, 0) << "Output device should have output channels";
        EXPECT_GT(device.preferred_sample_rate, 0) << "Device should have valid sample rate";
        EXPECT_FALSE(device.name.empty()) << "Device should have a name";
        EXPECT_FALSE(device.supported_samplerates.empty()) << "Device should support some sample rates";

        std::cout << "Output Device: " << device.name
                  << " (" << device.output_channels << " channels, "
                  << device.preferred_sample_rate << "Hz)" << std::endl;
    }

    for (const auto& device : input_devices) {
        EXPECT_GT(device.input_channels, 0) << "Input device should have input channels";
        EXPECT_FALSE(device.name.empty()) << "Input device should have a name";

        std::cout << "Input Device: " << device.name
                  << " (" << device.input_channels << " channels)" << std::endl;
    }

    unsigned int default_output = device_manager->get_default_output_device();
    unsigned int default_input = device_manager->get_default_input_device();

    std::cout << "Default output device ID: " << default_output << std::endl;
    std::cout << "Default input device ID: " << default_input << std::endl;
}

TEST_F(AudioBackendTest, DuplexCapabilityDetection)
{
    const auto& output_devices = device_manager->get_output_devices();
    const auto& input_devices = device_manager->get_input_devices();

    std::vector<Core::DeviceInfo> duplex_devices;
    for (const auto& out_dev : output_devices) {
        for (const auto& in_dev : input_devices) {
            if (out_dev.name == in_dev.name && out_dev.duplex_channels > 0) {
                duplex_devices.push_back(out_dev);
                break;
            }
        }
    }

    std::cout << "Found " << duplex_devices.size() << " duplex-capable devices" << std::endl;

    for (const auto& device : duplex_devices) {
        EXPECT_GT(device.duplex_channels, 0);
        EXPECT_GT(device.input_channels, 0);
        EXPECT_GT(device.output_channels, 0);

        std::cout << "Duplex Device: " << device.name
                  << " (duplex: " << device.duplex_channels << ")" << std::endl;
    }
}

//-------------------------------------------------------------------------
// Audio Stream Lifecycle Tests
//-------------------------------------------------------------------------

class AudioStreamTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        backend = std::make_unique<Core::RtAudioBackend>();
        device_manager = backend->create_device_manager();

        stream_info.sample_rate = TestConfig::SAMPLE_RATE;
        stream_info.buffer_size = TestConfig::BUFFER_SIZE;
        stream_info.output.channels = TestConfig::NUM_CHANNELS;
        stream_info.input.enabled = false;

        output_device_id = device_manager->get_default_output_device();
        input_device_id = device_manager->get_default_input_device();
    }

    void TearDown() override
    {
        if (stream && stream->is_running()) {
            stream->stop();
        }
        if (stream && stream->is_open()) {
            stream->close();
        }
        stream.reset();
        device_manager.reset();
        backend.reset();
    }

    std::unique_ptr<Core::IAudioBackend> backend;
    std::unique_ptr<Core::AudioDevice> device_manager;
    std::unique_ptr<Core::AudioStream> stream;
    Core::GlobalStreamInfo stream_info;
    unsigned int output_device_id;
    unsigned int input_device_id;
};

TEST_F(AudioStreamTest, StreamCreationAndDestruction)
{
    EXPECT_NO_THROW({
        stream = backend->create_stream(output_device_id, input_device_id, stream_info, nullptr);
    });

    ASSERT_NE(stream, nullptr);
    EXPECT_FALSE(stream->is_open());
    EXPECT_FALSE(stream->is_running());

    EXPECT_NO_THROW(stream->open());
    EXPECT_TRUE(stream->is_open());
    EXPECT_FALSE(stream->is_running());

    EXPECT_NO_THROW(stream->start());
    EXPECT_TRUE(stream->is_open());
    EXPECT_TRUE(stream->is_running());

    EXPECT_NO_THROW(stream->stop());
    EXPECT_TRUE(stream->is_open());
    EXPECT_FALSE(stream->is_running());

    EXPECT_NO_THROW(stream->close());
    EXPECT_FALSE(stream->is_open());
    EXPECT_FALSE(stream->is_running());
}

TEST_F(AudioStreamTest, StreamStateTransitions)
{
    stream = backend->create_stream(output_device_id, input_device_id, stream_info, nullptr);

    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(stream->open());
        EXPECT_TRUE(stream->is_open());

        EXPECT_NO_THROW(stream->start());
        EXPECT_TRUE(stream->is_running());

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        EXPECT_NO_THROW(stream->stop());
        EXPECT_FALSE(stream->is_running());

        EXPECT_NO_THROW(stream->close());
        EXPECT_FALSE(stream->is_open());
    }
}

TEST_F(AudioStreamTest, InputEnabledStreamCreation)
{
    stream_info.input.enabled = true;
    stream_info.input.channels = 1;

    EXPECT_NO_THROW({
        stream = backend->create_stream(output_device_id, input_device_id, stream_info, nullptr);
    });

    ASSERT_NE(stream, nullptr);

    EXPECT_NO_THROW(stream->open());

    if (stream->is_open()) {
        std::cout << "Successfully opened input-enabled stream" << std::endl;
        EXPECT_NO_THROW(stream->start());

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        EXPECT_NO_THROW(stream->stop());
        EXPECT_NO_THROW(stream->close());
    } else {
        std::cout << "Input-enabled stream failed to open (likely no input hardware)" << std::endl;
    }
}

//-------------------------------------------------------------------------
// Audio Subsystem Integration Tests
//-------------------------------------------------------------------------

/*
class AudioSubsystemTest : public ::testing::Test {
protected:
void SetUp() override
{
    engine = AudioTestHelper::createTestEngine();
}

void TearDown() override
{
    if (engine) {
        engine->End();
    }
    engine.reset();
}

std::unique_ptr<Core::Engine> engine;
};

TEST_F(AudioSubsystemTest, SubsystemInitialization)
{
auto subsystem_manager = engine->get_subsystem_manager();
ASSERT_NE(subsystem_manager, nullptr);

auto audio_subsystem = subsystem_manager->get_audio_subsystem();
ASSERT_NE(audio_subsystem, nullptr);

EXPECT_EQ(audio_subsystem->get_type(), Core::SubsystemType::AUDIO);
EXPECT_TRUE(audio_subsystem->is_ready());

auto* backend = audio_subsystem->get_audio_backend();
EXPECT_NE(backend, nullptr);

auto* device_manager = audio_subsystem->get_device_manager();
EXPECT_NE(device_manager, nullptr);

auto* stream_manager = audio_subsystem->get_stream_manager();
EXPECT_NE(stream_manager, nullptr);

const auto& stream_info = audio_subsystem->get_stream_info();
EXPECT_EQ(stream_info.sample_rate, TestConfig::SAMPLE_RATE);
EXPECT_EQ(stream_info.buffer_size, TestConfig::BUFFER_SIZE);
EXPECT_EQ(stream_info.output.channels, TestConfig::NUM_CHANNELS);
}

TEST_F(AudioSubsystemTest, AudioProcessingPipeline)
{
EXPECT_NO_THROW(engine->Start());

auto audio_subsystem = engine->get_subsystem_manager()->get_audio_subsystem();
ASSERT_NE(audio_subsystem, nullptr);

std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);

EXPECT_NO_THROW({
    int result = audio_subsystem->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);
    std::cout << "Audio output processing result: " << result << std::endl;
});

EXPECT_EQ(output_buffer.size(), TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS);
}

TEST_F(AudioSubsystemTest, NodeGraphIntegration)
{
EXPECT_NO_THROW(engine->Start());

auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
auto node_graph = engine->get_node_graph_manager();
ASSERT_NE(node_graph, nullptr);

EXPECT_NO_THROW(node_graph->add_to_root(sine, Nodes::ProcessingToken::AUDIO_RATE));

auto audio_subsystem = engine->get_subsystem_manager()->get_audio_subsystem();

std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);

EXPECT_NO_THROW({
    audio_subsystem->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);
});

bool has_signal = std::any_of(output_buffer.begin(), output_buffer.end(),
    [](double sample) { return std::abs(sample) > 0.01; });

if (has_signal) {
    std::cout << "Audio signal detected in output" << std::endl;
} else {
    std::cout << "No audio signal detected (may be normal in CI)" << std::endl;
}

EXPECT_NO_THROW(node_graph->get_root_node(Nodes::ProcessingToken::AUDIO_RATE, 0).unregister_node(sine));
} */

#ifdef AUDIBLE_TEST
//-------------------------------------------------------------------------
// Audible Output Tests (Manual Testing)
//-------------------------------------------------------------------------

class AudibleTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::cout << "=========================================" << std::endl;
        std::cout << "STARTING AUDIBLE TEST - YOU SHOULD HEAR AUDIO" << std::endl;
        std::cout << "=========================================" << std::endl;

        engine = AudioTestHelper::createTestEngine();
        EXPECT_NO_THROW(engine->Start());
    }

    void TearDown() override
    {
        if (engine) {
            engine->End();
        }
        engine.reset();

        std::cout << "=========================================" << std::endl;
        std::cout << "AUDIBLE TEST COMPLETE" << std::endl;
        std::cout << "=========================================" << std::endl;
    }

    void waitForAudio(unsigned int ms = 2000)
    {
        std::cout << "Playing audio for " << ms << "ms..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    std::unique_ptr<Core::Engine> engine;
};

TEST_F(AudibleTest, SineWaveOutput)
{
    std::cout << "You should hear a 440Hz sine wave..." << std::endl;

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto node_graph = engine->get_node_graph_manager();

    EXPECT_NO_THROW(node_graph->add_to_root(sine, Nodes::ProcessingToken::AUDIO_RATE));

    AudioTestHelper::waitForAudio(500);

    auto& root = node_graph->get_root_node(Nodes::ProcessingToken::AUDIO_RATE, 0);
    std::cout << "Node count in graph: " << root.get_node_size() << std::endl;
    EXPECT_EQ(root.get_node_size(), 1);

    waitForAudio(500);

    std::cout << "Changing frequency to 880Hz..." << std::endl;
    sine->set_frequency(880.0f);
    waitForAudio(500);

    std::cout << "Changing frequency to 220Hz..." << std::endl;
    sine->set_frequency(220.0f);
    waitForAudio(500);

    EXPECT_NO_THROW(root.unregister_node(sine));
    std::cout << "Sine wave removed. Should now hear silence." << std::endl;
    waitForAudio(500);
}

TEST_F(AudibleTest, FilteredAudioOutput)
{
    std::cout << "You should hear a filtered sine wave..." << std::endl;

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.7f);
    std::vector<double> coeffs(5, 0.2);
    auto filter = std::make_shared<Nodes::Filters::FIR>(sine, coeffs);

    auto node_graph = engine->get_node_graph_manager();
    EXPECT_NO_THROW(node_graph->add_to_root(filter, Nodes::ProcessingToken::AUDIO_RATE));

    auto& root = node_graph->get_root_node(Nodes::ProcessingToken::AUDIO_RATE, 0);
    std::cout << "Filter node added to graph. Node count: " << root.get_node_size() << std::endl;

    waitForAudio(1000);

    EXPECT_NO_THROW(root.unregister_node(filter));
    waitForAudio(1000);
}

TEST_F(AudibleTest, NoiseGeneratorOutput)
{
    std::cout << "Testing various noise types..." << std::endl;

    auto noise = std::make_shared<Nodes::Generator::Stochastics::Random>();
    noise->set_amplitude(0.3f);

    auto node_graph = engine->get_node_graph_manager();
    EXPECT_NO_THROW(node_graph->add_to_root(noise, Nodes::ProcessingToken::AUDIO_RATE));

    struct NoiseTest {
        Kinesis::Stochastic::Algorithm type;
        std::string name;
    };

    std::vector<NoiseTest> noiseTypes = {
        { Kinesis::Stochastic::Algorithm::UNIFORM, "Uniform" },
        { Kinesis::Stochastic::Algorithm::NORMAL, "Normal (Gaussian)" },
        { Kinesis::Stochastic::Algorithm::EXPONENTIAL, "Exponential" }
    };

    for (const auto& test : noiseTypes) {
        std::cout << "Playing " << test.name << " noise..." << std::endl;
        noise->set_type(test.type);
        waitForAudio(1000);
    }

    auto& root = node_graph->get_root_node(Nodes::ProcessingToken::AUDIO_RATE, 0);
    EXPECT_NO_THROW(root.unregister_node(noise));
}

#endif // AUDIBLE_TEST

#endif // INTEGRATION_TEST

//-------------------------------------------------------------------------
// RtAudio Singleton and Backend Utility Tests (Always Enabled)
//-------------------------------------------------------------------------

class RtAudioUtilityTest : public ::testing::Test {
protected:
    void SetUp() override { }

    void TearDown() override { }
};

TEST_F(RtAudioUtilityTest, SingletonAccess)
{
    RtAudio* context1 = Core::RtAudioSingleton::get_instance();
    RtAudio* context2 = Core::RtAudioSingleton::get_instance();

    EXPECT_NE(context1, nullptr);
    EXPECT_EQ(context1, context2) << "Singleton should return same instance";

    unsigned int device_count = context1->getDeviceCount();
    std::string version = context1->getVersion();
    RtAudio::Api api = context1->getCurrentApi();

    std::cout << "RtAudio Device Count: " << device_count << std::endl;
    std::cout << "RtAudio Version: " << version << std::endl;
    std::cout << "RtAudio API: " << api << std::endl;

    EXPECT_FALSE(version.empty());
    EXPECT_NE(api, RtAudio::Api::UNSPECIFIED);
}

TEST_F(RtAudioUtilityTest, StreamExclusivity)
{
    EXPECT_NO_THROW(Core::RtAudioSingleton::mark_stream_open());

    EXPECT_THROW(Core::RtAudioSingleton::mark_stream_open(), std::runtime_error);

    Core::RtAudioSingleton::mark_stream_closed();
    EXPECT_NO_THROW(Core::RtAudioSingleton::mark_stream_open());

    Core::RtAudioSingleton::mark_stream_closed();
}

TEST_F(RtAudioUtilityTest, GlobalAPIValidation)
{
    MayaFlux::End();

    MayaFlux::Init(44100, 256, 1, 0);
    EXPECT_TRUE(MayaFlux::is_engine_initialized());
    EXPECT_EQ(Config::get_sample_rate(), 44100);
    EXPECT_EQ(Config::get_buffer_size(), 256);
    EXPECT_EQ(Config::get_num_out_channels(), 1);

    MayaFlux::End();

    Core::GlobalStreamInfo duplex_config;
    duplex_config.sample_rate = 48000;
    duplex_config.buffer_size = 512;
    duplex_config.output.channels = 2;
    duplex_config.input.enabled = true;
    duplex_config.input.channels = 1;

    MayaFlux::Init(duplex_config, {}, {});

    auto stream_info = Config::get_global_stream_info();
    EXPECT_EQ(stream_info.sample_rate, 48000);
    EXPECT_EQ(stream_info.output.channels, 2);
    EXPECT_TRUE(stream_info.input.enabled);
    EXPECT_EQ(stream_info.input.channels, 1);

    MayaFlux::End();
}

TEST_F(RtAudioUtilityTest, BackendFactoryValidation)
{
    auto backend = Core::AudioBackendFactory::create_backend(Core::AudioBackendType::RTAUDIO);
    EXPECT_NE(backend, nullptr);

    EXPECT_FALSE(backend->get_version_string().empty());
    EXPECT_NE(backend->get_api_type(), RtAudio::Api::UNSPECIFIED);

    // Test device manager creation
    auto device_manager = backend->create_device_manager();
    EXPECT_NE(device_manager, nullptr);
}

} // namespace MayaFlux::Test
