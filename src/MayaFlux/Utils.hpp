#pragma once

#include "config.h"

namespace MayaFlux::Utils {

enum NodeProcessType {
    OneSample,
    Filter,
    Buffer
};

enum class distribution {
    UNIFORM,
    NORMAL,
    EXPONENTIAL,
    POISSON,
    // PERLIN,
    // GENDY
};

enum coefficients {
    INPUT,
    OUTPUT,
    ALL
};

enum ActionType {
    NODE,
    TIME,
    FUNCTION
};

}
