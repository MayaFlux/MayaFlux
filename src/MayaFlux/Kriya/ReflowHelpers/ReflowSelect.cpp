#include "ReflowSelect.hpp"

namespace MayaFlux::Kriya::Detail {

size_t nearest_frame(const Kinesis::TimeLocation& location)
{
    return location.fraction >= 0.5 ? location.upper : location.lower;
}

FramePicks select_frames(const Kinesis::TimeLocation& location, FrameSelection selection, double weight)
{
    FramePicks picks;
    if (!std::isfinite(weight) || !(weight > 0.0)) {
        return picks;
    }

    switch (selection) {
    case FrameSelection::Floor:
        picks.items[0] = { location.lower, weight };
        picks.count = 1;
        break;
    case FrameSelection::Nearest:
        picks.items[0] = { nearest_frame(location), weight };
        picks.count = 1;
        break;
    case FrameSelection::Blend: {
        const double fraction = std::clamp(location.fraction, 0.0, 1.0);
        if (location.upper == location.lower || fraction <= 0.0) {
            picks.items[0] = { location.lower, weight };
            picks.count = 1;
        } else if (fraction >= 1.0) {
            picks.items[0] = { location.upper, weight };
            picks.count = 1;
        } else {
            picks.items[0] = { location.lower, weight * (1.0 - fraction) };
            picks.items[1] = { location.upper, weight * fraction };
            picks.count = 2;
        }
        break;
    }
    }
    return picks;
}

} // namespace MayaFlux::Kriya::Detail
