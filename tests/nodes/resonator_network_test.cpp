#include "../test_config.h"

#include "MayaFlux/Nodes/Generators/Polynomial.hpp"
#include "MayaFlux/Nodes/Network/ResonatorNetwork.hpp"

namespace MayaFlux::Test {

using Nodes::Generator::Polynomial;
using Nodes::Network::ResonatorNetwork;

namespace {

    std::shared_ptr<Nodes::Node> make_pulse()
    {
        auto fired = std::make_shared<bool>(false);
        return std::make_shared<Polynomial>([fired](double) {
            if (*fired) {
                return 0.0;
            }
            *fired = true;
            return 1.0;
        });
    }

    std::shared_ptr<ResonatorNetwork> make_single(double frequency, double q)
    {
        auto net = std::make_shared<ResonatorNetwork>(
            std::vector<double> { frequency }, std::vector<double> { q });
        net->set_exciter(make_pulse());
        return net;
    }

    std::vector<double> render(const std::shared_ptr<ResonatorNetwork>& net, unsigned int samples)
    {
        net->process_batch(samples);
        return net->get_audio_buffer().value_or(std::vector<double> {});
    }

    double rms(const std::vector<double>& y, size_t begin, size_t length)
    {
        double acc = 0.0;
        for (size_t n = begin; n < begin + length; ++n) {
            acc += y.at(n) * y.at(n);
        }
        return std::sqrt(acc / static_cast<double>(length));
    }

    double peak(const std::vector<double>& y)
    {
        double p = 0.0;
        for (const double v : y) {
            p = std::max(p, std::abs(v));
        }
        return p;
    }

    using Formants = std::array<std::pair<double, double>, 5>;

    struct VowelCase {
        const char* name;
        ResonatorNetwork::FormantPreset preset;
        Formants formants;
    };

}

class ResonatorPresetTest : public ::testing::TestWithParam<VowelCase> { };

TEST_P(ResonatorPresetTest, ConstructorLoadsFormantTable)
{
    const auto& vowel = GetParam();
    ResonatorNetwork net(5, vowel.preset);

    ASSERT_EQ(net.get_node_count(), 5U);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_DOUBLE_EQ(net.get_resonators().at(i).frequency, vowel.formants.at(i).first) << "i=" << i;
        EXPECT_DOUBLE_EQ(net.get_resonators().at(i).q, vowel.formants.at(i).second) << "i=" << i;
    }
}

TEST_P(ResonatorPresetTest, ApplyPresetReloadsTableOverCustomValues)
{
    const auto& vowel = GetParam();
    ResonatorNetwork net(
        std::vector<double> { 100.0, 200.0, 300.0, 400.0, 500.0 },
        std::vector<double> { 5.0, 5.0, 5.0, 5.0, 5.0 });

    net.apply_preset(vowel.preset);

    for (size_t i = 0; i < 5; ++i) {
        EXPECT_DOUBLE_EQ(net.get_resonators().at(i).frequency, vowel.formants.at(i).first) << "i=" << i;
        EXPECT_DOUBLE_EQ(net.get_resonators().at(i).q, vowel.formants.at(i).second) << "i=" << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Vowels,
    ResonatorPresetTest,
    ::testing::Values(
        VowelCase { "A", ResonatorNetwork::FormantPreset::VOWEL_A, { { { 800.0, 16.0 }, { 1200.0, 30.0 }, { 2500.0, 55.0 }, { 3500.0, 70.0 }, { 4500.0, 90.0 } } } },
        VowelCase { "E", ResonatorNetwork::FormantPreset::VOWEL_E, { { { 400.0, 10.0 }, { 2000.0, 45.0 }, { 2600.0, 55.0 }, { 3500.0, 70.0 }, { 4500.0, 90.0 } } } },
        VowelCase { "I", ResonatorNetwork::FormantPreset::VOWEL_I, { { { 270.0, 7.0 }, { 2300.0, 50.0 }, { 3000.0, 60.0 }, { 3500.0, 70.0 }, { 4500.0, 90.0 } } } },
        VowelCase { "O", ResonatorNetwork::FormantPreset::VOWEL_O, { { { 500.0, 12.0 }, { 900.0, 22.0 }, { 2500.0, 55.0 }, { 3500.0, 70.0 }, { 4500.0, 90.0 } } } },
        VowelCase { "U", ResonatorNetwork::FormantPreset::VOWEL_U, { { { 300.0, 8.0 }, { 800.0, 20.0 }, { 2300.0, 50.0 }, { 3500.0, 70.0 }, { 4500.0, 90.0 } } } }),
    [](const ::testing::TestParamInfo<VowelCase>& info) { return std::string(info.param.name); });

TEST(ResonatorPresetExtraTest, ResonatorsBeyondTheTableKeepDefaults)
{
    ResonatorNetwork net(7, ResonatorNetwork::FormantPreset::VOWEL_A);

    EXPECT_DOUBLE_EQ(net.get_resonators().at(5).frequency, 440.0);
    EXPECT_DOUBLE_EQ(net.get_resonators().at(5).q, 10.0);
    EXPECT_DOUBLE_EQ(net.get_resonators().at(6).frequency, 440.0);
}

TEST(ResonatorExcitationTest, SilentWithoutExciter)
{
    auto net = std::make_shared<ResonatorNetwork>(
        std::vector<double> { 1000.0 }, std::vector<double> { 30.0 });

    const auto y = render(net, 512);

    ASSERT_EQ(y.size(), 512U);
    EXPECT_EQ(peak(y), 0.0);
}

TEST(ResonatorExcitationTest, ImpulseRingsAtDesignFrequencyAndDies)
{
    constexpr double frequency = 1200.0;
    auto net = make_single(frequency, 30.0);
    const double sample_rate = net->get_sample_rate();

    const auto y = render(net, 8192);

    size_t crossings = 0;
    for (size_t n = 1; n < 400; ++n) {
        if ((y.at(n - 1) < 0.0) != (y.at(n) < 0.0)) {
            ++crossings;
        }
    }
    const double measured = static_cast<double>(crossings) * 0.5 * sample_rate / 399.0;
    EXPECT_NEAR(measured, frequency, 0.05 * frequency);

    EXPECT_LT(rms(y, y.size() - 256, 256), 1e-4 * rms(y, 0, 400));
}

TEST(ResonatorExcitationTest, ResonatorExciterDrivesOnlyItsOwnVoice)
{
    ResonatorNetwork net(
        std::vector<double> { 800.0, 1500.0 }, std::vector<double> { 30.0, 30.0 });
    net.set_resonator_exciter(1, make_pulse());

    net.process_batch(512);

    const auto first = net.get_node_audio_buffer(0);
    const auto second = net.get_node_audio_buffer(1);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());

    double first_peak = 0.0;
    double second_peak = 0.0;
    for (const double v : *first) {
        first_peak = std::max(first_peak, std::abs(v));
    }
    for (const double v : *second) {
        second_peak = std::max(second_peak, std::abs(v));
    }
    EXPECT_EQ(first_peak, 0.0);
    EXPECT_GT(second_peak, 1e-4);
}

TEST(ResonatorGainTest, GainScalesOutputLinearly)
{
    auto unit = make_single(1000.0, 30.0);
    auto doubled = make_single(1000.0, 30.0);
    doubled->set_resonator_gain(0, 2.0);

    const auto a = render(unit, 512);
    const auto b = render(doubled, 512);

    ASSERT_EQ(a.size(), b.size());
    for (size_t n = 0; n < a.size(); ++n) {
        EXPECT_NEAR(b.at(n), 2.0 * a.at(n), 1e-12) << "n=" << n;
    }
}

TEST(ResonatorRetuneTest, SetFrequencyClearsStateRetuneKeepsIt)
{
    auto cleared = make_single(1200.0, 30.0);
    auto kept = make_single(1200.0, 30.0);

    render(cleared, 256);
    render(kept, 256);

    cleared->set_frequency(0, 1250.0);
    kept->retune(0, 1250.0);

    const auto after_clear = render(cleared, 1);
    const auto after_retune = render(kept, 1);

    EXPECT_EQ(after_clear.at(0), 0.0);
    EXPECT_GT(std::abs(after_retune.at(0)), 1e-9);
}

TEST(ResonatorRetuneTest, RetuneWritesTheSameDesignAsAFreshResonator)
{
    auto retuned = make_single(1000.0, 30.0);
    retuned->retune(0, 1500.0);

    ResonatorNetwork fresh(std::vector<double> { 1500.0 }, std::vector<double> { 30.0 });

    const auto& got = retuned->get_resonators().at(0);
    const auto& want = fresh.get_resonators().at(0);

    EXPECT_DOUBLE_EQ(got.frequency, 1500.0);
    ASSERT_EQ(got.filter->getACoefficients().size(), want.filter->getACoefficients().size());
    for (size_t i = 0; i < got.filter->getACoefficients().size(); ++i) {
        EXPECT_NEAR(got.filter->getACoefficients().at(i), want.filter->getACoefficients().at(i), 1e-12) << "a" << i;
    }
    for (size_t i = 0; i < got.filter->getBCoefficients().size(); ++i) {
        EXPECT_NEAR(got.filter->getBCoefficients().at(i), want.filter->getBCoefficients().at(i), 1e-12) << "b" << i;
    }
}

TEST(ResonatorDecayTest, RingEnvelopeFallsByOneOverEPerDecayTime)
{
    constexpr double frequency = 1000.0;
    constexpr double decay = 0.05;
    auto net = make_single(frequency, 30.0);
    net->set_decay(0, decay);
    const double sample_rate = net->get_sample_rate();

    const auto period = static_cast<size_t>(sample_rate / frequency);
    const auto early = static_cast<size_t>(0.02 * sample_rate);
    const auto late = early + static_cast<size_t>(decay * sample_rate);

    const auto y = render(net, static_cast<unsigned int>(late + period));

    const double ratio = rms(y, late, period) / rms(y, early, period);
    EXPECT_NEAR(ratio, std::exp(-1.0), 0.1 * std::exp(-1.0));
}

TEST(ResonatorDecayTest, DecayIsNotLimitedToTheQRange)
{
    auto net = make_single(1000.0, 30.0);

    net->set_decay(0, 2.0);

    EXPECT_GT(net->get_resonators().at(0).q, 1000.0);
    EXPECT_LT(net->get_resonators().at(0).filter->max_pole_magnitude(), 1.0);
}

TEST(ResonatorDecayTest, ExtremeDecayStaysStable)
{
    auto net = make_single(1000.0, 30.0);

    net->set_decay(0, 1e12);

    EXPECT_LT(net->get_resonators().at(0).filter->max_pole_magnitude(), 1.0);
}

TEST(ResonatorDecayTest, SetDecayKeepsRingingState)
{
    auto net = make_single(1200.0, 30.0);
    render(net, 256);

    net->set_decay(0, 0.01);
    const auto after = render(net, 1);

    EXPECT_GT(std::abs(after.at(0)), 1e-9);
}

TEST(ResonatorRangeTest, RetuneAndSetDecayRejectBadIndex)
{
    ResonatorNetwork net(2, ResonatorNetwork::FormantPreset::NONE);

    EXPECT_THROW(net.retune(2, 500.0), std::out_of_range);
    EXPECT_THROW(net.set_decay(2, 0.5), std::out_of_range);
}

}
