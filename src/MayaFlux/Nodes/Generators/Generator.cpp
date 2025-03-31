#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator {

std::vector<double> HannWindow(size_t length)
{
    std::vector<double> window(length);
    if (length == 1)
        return { 1.0 };
    const double scale = 1.0 / (length - 1);
    for (size_t i = 0; i < length; ++i) {
        window[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i * scale));
    }
    return window;
}

std::vector<double> HammingWindow(size_t length)
{
    std::vector<double> window(length);
    if (length == 1)
        return { 1.0 };
    const double scale = 1.0 / (length - 1);
    for (size_t i = 0; i < length; ++i) {
        window[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i * scale);
    }
    return window;
}

std::vector<double> BlackmanWindow(size_t length)
{
    std::vector<double> window(length);
    if (length == 1)
        return { 1.0 };
    const double scale = 1.0 / (length - 1);
    for (size_t i = 0; i < length; ++i) {
        const double x = 2.0 * M_PI * i * scale;
        window[i] = 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x);
    }
    return window;
}

std::vector<double> LinearRamp(size_t length, double start, double end)
{
    std::vector<double> ramp(length);
    if (length == 1)
        return { start };
    const double step = (end - start) / (length - 1);
    for (size_t i = 0; i < length; ++i) {
        ramp[i] = start + i * step;
    }
    return ramp;
}

std::vector<double> ExponentialRamp(size_t length, double start, double end)
{
    std::vector<double> ramp(length);
    if (length == 1)
        return { start };
    const double growth = std::pow(end / start, 1.0 / (length - 1));
    ramp[0] = start;
    for (size_t i = 1; i < length; ++i) {
        ramp[i] = ramp[i - 1] * growth;
    }
    return ramp;
}
}
