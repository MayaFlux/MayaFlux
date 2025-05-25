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

enum MF_NodeState
{
    MFOP_INVALID = 0x0,
    MFOP_ACTIVE = 0x01,
    MFOP_IS_PROCESSING = 0x02,
    //MFOP_IS_ADD = 0x04,
};

__forceinline void AtomicSetFlagStrong(std::atomic<MF_NodeState>& flag, MF_NodeState& expected, const MF_NodeState& desired)
{
    flag.compare_exchange_strong(expected, desired);
};

__forceinline void AtomicSetFlagStrong(std::atomic<MF_NodeState>& flag, const MF_NodeState& desired)
{
    auto expected = flag.load();
    flag.compare_exchange_strong(expected, desired);
};

__forceinline void AtomicSetFlagWeak(std::atomic<MF_NodeState>& flag, MF_NodeState& expected, const MF_NodeState& desired)
{
    flag.compare_exchange_weak(expected, desired);
};


}
