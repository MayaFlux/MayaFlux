#include "../test_config.h"

#include "MayaFlux/Kinesis/Discrete/Kernels.hpp"
#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Kinesis/Tendency/TendencyFactories.hpp"
#include "MayaFlux/Kinesis/Tendency/TimeMap.hpp"

namespace MayaFlux::Test {

TEST(TabulateTest, SamplesEvenlyIncludingBothEnds)
{
    const auto path = Kinesis::TimeMaps::linear(0.0, 10.0, 1.0);

    EXPECT_EQ(Kinesis::tabulate(path, 3, 0.0, 1.0), (std::vector<double> { 0.0, 5.0, 10.0 }));
    EXPECT_EQ(Kinesis::tabulate(path, 1, 0.5, 1.0), (std::vector<double> { 5.0 }));
    EXPECT_TRUE(Kinesis::tabulate(path, 0).empty());
}

TEST(TabulateTest, ATabulatedCurveIsReadBackByTableLookup)
{
    const Kinesis::TimeMap squared { .fn = [](const double& x) { return x * x; } };
    const auto table = Kinesis::tabulate(squared, 201);

    for (const double x : { -1.0, -0.5, 0.0, 0.3, 1.0 }) {
        EXPECT_NEAR(Kinesis::Discrete::table_lookup(std::vector<double> { x }, table), x * x, 1e-4) << "x=" << x;
    }
}

}
