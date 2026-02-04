#pragma once

namespace MayaFlux::Kinesis {

enum class distribution : uint8_t {
    UNIFORM,
    NORMAL,
    EXPONENTIAL,
    POISSON,
    // PERLIN,
    // GENDY
};

}
