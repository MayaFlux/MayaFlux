#include "VisionAnalyzer.hpp"
#include <numeric>

namespace MayaFlux::Yantra {

double extract_scalar_vision(
    const VisionAnalysis& analysis, const std::string& qualifier)
{
    const auto& f = analysis.frame;

    if (qualifier == "box_count")
        return static_cast<double>(f.boxes.size());
    if (qualifier == "contour_count")
        return static_cast<double>(f.contours.size());
    if (qualifier == "keypoint_count")
        return static_cast<double>(f.keypoints.size());
    if (qualifier == "track_count")
        return static_cast<double>(f.tracks.size());
    if (qualifier == "component_count")
        return static_cast<double>(f.components.count);
    if (qualifier == "mean_gradient_magnitude") {
        const auto& mag = f.gradient.magnitude;
        if (mag.empty())
            return 0.0;
        return std::accumulate(mag.begin(), mag.end(), 0.0)
            / static_cast<double>(mag.size());
    }

    return 0.0;
}

} // namespace MayaFlux::Yantra
