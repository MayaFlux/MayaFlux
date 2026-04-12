#pragma once

namespace MayaFlux::Kakshya {

class DynamicSoundStream;

/**
 * @struct StreamSlice
 * @brief Plain data describing the state of one bounded stream read operation.
 *
 * Cursor and loop boundaries are in frames. Speed is a rate multiplier applied
 * to cursor advancement per block; values other than 1.0 require fractional
 * accumulation via cursor_remainder. Index identifies the slot within a
 * multi-slice processor and determines mix priority. Scale is an independent
 * per-slice amplitude multiplier applied at mix time.
 */
struct StreamSlice {
    std::shared_ptr<DynamicSoundStream> stream;

    uint64_t cursor { 0 };
    double cursor_remainder { 0.0 };
    double speed { 1.0 };

    uint64_t loop_start { 0 };
    uint64_t loop_end { 0 };

    bool looping { false };
    bool active { false };

    uint8_t index { 0 };
    double scale { 1.0 };
};

} // namespace MayaFlux::Kakshya
