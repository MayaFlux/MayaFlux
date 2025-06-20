#pragma once

#include "config.h"

namespace MayaFlux::Utils {

enum BackendType {
    RTAUDIO
};

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

enum NodeState : u_int32_t {
    INACTIVE = 0x00, ///< Engine is not processing this node
    ACTIVE = 0x01, ///< Engine is processing this node
    PENDING_REMOVAL = 0x02, ///< Node is marked for removal

    MOCK_PROCESS = 0x04, ///< Node should be processed but output ignored
    PROCESSED = 0x08, ///< Node has been processed this cycle

    ENGINE_PROCESSED = ACTIVE | PROCESSED, ///< Engine has processed this node
    EXTERMAL_PROCESSED = INACTIVE | PROCESSED, ///< External source has processed this node
    ENGINE_MOCK_PROCESSED = ACTIVE | MOCK_PROCESS | PROCESSED, ///< Engine has mock processed this node
};

std::any safe_get_parameter(const std::string& parameter_name, const std::map<std::string, std::any> parameters);
}
