#include "../test_config.h"

#include "MayaFlux/Kinesis/Differential.hpp"
#include "MayaFlux/Kinesis/SymbolicTrajectory.hpp"

namespace MayaFlux::Test {

using Kinesis::AABB1D;
using Kinesis::Lattice1D;

namespace {

    std::span<const double> view(const std::vector<double>& values)
    {
        return std::span<const double>(values);
    }

}

TEST(AABB1DTest, ContainmentOverlapAndShifts)
{
    const AABB1D a { .min = 0.0, .max = 2.0 };

    EXPECT_TRUE(a.contains(0.0));
    EXPECT_TRUE(a.contains(2.0));
    EXPECT_FALSE(a.contains(2.1));
    EXPECT_TRUE(a.overlaps({ .min = 1.5, .max = 3.0 }));
    EXPECT_FALSE(a.overlaps({ .min = 2.5, .max = 3.0 }));
    EXPECT_DOUBLE_EQ(a.width(), 2.0);
    EXPECT_DOUBLE_EQ(a.center(), 1.0);
    EXPECT_DOUBLE_EQ(a.translated(1.0).min, 1.0);
    EXPECT_DOUBLE_EQ(a.expanded(0.5).max, 2.5);
}

TEST(Lattice1DTest, CellAtMapsAndClampsToTheEdgeCells)
{
    const Lattice1D lattice { .resolution = 4U, .bounds = { .min = 0.0, .max = 1.0 } };

    EXPECT_EQ(lattice.cell_at(0.0), 0U);
    EXPECT_EQ(lattice.cell_at(0.26), 1U);
    EXPECT_EQ(lattice.cell_at(0.99), 3U);
    EXPECT_EQ(lattice.cell_at(1.0), 3U);
    EXPECT_EQ(lattice.cell_at(-5.0), 0U);
    EXPECT_EQ(lattice.cell_at(5.0), 3U);
}

TEST(Lattice1DTest, GeometryAgreesWithCellAt)
{
    const Lattice1D lattice { .resolution = 5U, .bounds = { .min = -1.0, .max = 1.0 } };

    EXPECT_DOUBLE_EQ(lattice.cell_size(), 0.4);
    EXPECT_EQ(lattice.cell_count(), 5U);
    EXPECT_EQ(lattice.corner_count(), 6U);
    EXPECT_DOUBLE_EQ(lattice.corner_position(5U), 1.0);

    for (uint32_t c = 0; c < lattice.resolution; ++c) {
        EXPECT_EQ(lattice.cell_at(lattice.cell_center(c)), c);
        EXPECT_TRUE(lattice.in_bounds(c));
    }
    EXPECT_FALSE(lattice.in_bounds(5U));
    EXPECT_EQ(lattice.resampled(10U).resolution, 10U);
}

TEST(Lattice1DTest, SymbolicTrajectoryObservesAScalarStream)
{
    Kinesis::SymbolicTrajectory<Lattice1D, uint32_t> trajectory(
        Lattice1D { .resolution = 2U, .bounds = { .min = 0.0, .max = 1.0 } }, 8);

    const std::vector<double> stream { 0.1, 0.2, 0.6, 0.7, 0.2 };
    std::vector<bool> crossed;
    for (const double value : stream) {
        crossed.push_back(trajectory.update(value));
    }

    EXPECT_EQ(trajectory.crossing_count(), 2U);
    EXPECT_EQ(trajectory.current_cell(), 0U);
    EXPECT_EQ(trajectory.dwell_count(), 1U);
    EXPECT_EQ(trajectory.unique_cells_in_window(5), 2U);
    EXPECT_FALSE(crossed.at(0));
    EXPECT_TRUE(crossed.at(2));
    EXPECT_TRUE(crossed.at(4));
}

TEST(ScalarDifferenceTest, SpanFormMatchesTheHistoryBufferForm)
{
    const std::vector<double> newest_first { 4.0, 1.0, 0.0, 3.0, -2.0, 5.0, 1.5, 0.5 };
    const auto history = Kinesis::to_history<double>(view(newest_first));

    EXPECT_DOUBLE_EQ(Kinesis::backward_difference<1>(view(newest_first), 0.01), Kinesis::backward_difference<1>(history, 0.01));
    EXPECT_DOUBLE_EQ(Kinesis::backward_difference<3>(view(newest_first), 0.01), Kinesis::backward_difference<3>(history, 0.01));
    EXPECT_DOUBLE_EQ(Kinesis::backward_difference<6>(view(newest_first), 0.01), Kinesis::backward_difference<6>(history, 0.01));
}

TEST(ScalarDifferenceTest, NamedOrdersOfAQuadratic)
{
    const std::vector<double> squares { 4.0, 1.0, 0.0 };

    EXPECT_DOUBLE_EQ(Kinesis::velocity(view(squares), 1.0), 3.0);
    EXPECT_DOUBLE_EQ(Kinesis::acceleration(view(squares), 1.0), 2.0);
    EXPECT_DOUBLE_EQ(Kinesis::acceleration(view(squares), 0.5), 8.0);
}

TEST(ScalarDifferenceTest, SamplesBeyondTheSpanReadAsZero)
{
    const std::vector<double> one { 5.0 };

    EXPECT_DOUBLE_EQ(Kinesis::velocity(view(one), 1.0), 5.0);
}

TEST(ScalarDifferenceTest, MovingAverageOverTheNewestSamples)
{
    const std::vector<double> values { 1.0, 2.0, 3.0, 4.0 };

    EXPECT_DOUBLE_EQ(Kinesis::moving_average(view(values), 2), 1.5);
    EXPECT_DOUBLE_EQ(Kinesis::moving_average(view(values), 4), 2.5);
}

TEST(ScalarWindowMeasureTest, PathLengthAndNetDisplacement)
{
    const std::vector<double> values { 5.0, 3.0, 4.0, 1.0 };

    EXPECT_DOUBLE_EQ(Kinesis::path_length(view(values), 4), 6.0);
    EXPECT_DOUBLE_EQ(Kinesis::net_displacement(view(values), 4), 4.0);
    EXPECT_DOUBLE_EQ(Kinesis::path_length(view(values), 2), 2.0);
}

TEST(ScalarWindowMeasureTest, StraightnessSeparatesMonotoneFromWandering)
{
    const std::vector<double> monotone { 4.0, 3.0, 2.0, 1.0 };
    const std::vector<double> wandering { 5.0, 3.0, 4.0, 1.0 };
    const std::vector<double> flat { 2.0, 2.0, 2.0 };

    EXPECT_DOUBLE_EQ(Kinesis::straightness(view(monotone), 4), 1.0);
    EXPECT_NEAR(Kinesis::straightness(view(wandering), 4), 4.0 / 6.0, 1e-12);
    EXPECT_DOUBLE_EQ(Kinesis::straightness(view(flat), 3), 0.0);
}

TEST(ScalarWindowMeasureTest, ReversalCountAndSpread)
{
    const std::vector<double> zigzag { 5.0, 3.0, 4.0, 1.0 };
    const std::vector<double> ramp { 4.0, 3.0, 2.0, 1.0 };
    const std::vector<double> values { 1.0, 3.0, 5.0 };

    EXPECT_EQ(Kinesis::reversal_count(view(zigzag), 4), 2U);
    EXPECT_EQ(Kinesis::reversal_count(view(ramp), 4), 0U);
    EXPECT_DOUBLE_EQ(Kinesis::spread_radius(view(values), 3), 2.0);
}

TEST(ScalarWindowMeasureTest, WindowsLongerThanTheSpanAreClamped)
{
    const std::vector<double> values { 2.0, 1.0 };

    EXPECT_DOUBLE_EQ(Kinesis::path_length(view(values), 100), 1.0);
    EXPECT_DOUBLE_EQ(Kinesis::net_displacement(view(values), 100), 1.0);
    EXPECT_DOUBLE_EQ(Kinesis::spread_radius(view(values), 100), 0.5);
}

}
