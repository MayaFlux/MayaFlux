#pragma once

#include "MayaFlux/Yantra/ComputeMatrix.hpp"

namespace MayaFlux::Kakshya {
class SignalSourceContainer;
}

namespace MayaFlux::Yantra {

/**
 * @class SignalTransformer
 * @brief Base class for operations that transform digital signal data
 *
 * Defines the interface for all signal processing operations that can be applied
 * to signal data containers. Extends the generic ComputeOperation with signal-specific
 * functionality like progress tracking and in-place processing capabilities.
 */
class SignalTransformer : public ComputeOperation<std::shared_ptr<Kakshya::SignalSourceContainer>,
                              std::shared_ptr<Kakshya::SignalSourceContainer>> {
public:
    /**
     * @brief Processes the input signal data
     * @param input Container with the signal data to transform
     * @return Container with the transformed signal data
     */
    virtual std::shared_ptr<Kakshya::SignalSourceContainer> apply_operation(
        std::shared_ptr<Kakshya::SignalSourceContainer> input) override
        = 0;

    /**
     * @brief Indicates whether the transformation modifies the input data directly
     * @return True if the operation modifies input in-place, false if it creates a new output
     */
    virtual bool is_in_place() const { return false; }

    /**
     * @brief Reports the current progress of a long-running transformation
     * @return Progress value between 0.0 (not started) and 1.0 (completed)
     */
    virtual double get_processing_progress() const { return 1.0; }
};

/**
 * @class TimeStretchTransformer
 * @brief Signal transformer that modifies the temporal characteristics of a signal
 *
 * Alters the duration of a signal without changing its frequency content,
 * effectively speeding up or slowing down the signal's playback rate.
 */
class TimeStretchTransformer : public SignalTransformer {
public:
    /**
     * @brief Constructs a time stretching transformer
     * @param stretch_factor Ratio of output duration to input duration
     *                      (>1.0 lengthens, <1.0 shortens)
     */
    TimeStretchTransformer(double stretch_factor = 1.0);

    /**
     * @brief Applies time stretching to the input signal
     * @param input Container with the signal data to stretch
     * @return Container with the time-stretched signal data
     */
    virtual std::shared_ptr<Kakshya::SignalSourceContainer> apply_operation(
        std::shared_ptr<Kakshya::SignalSourceContainer> input) override;

private:
    /** @brief Factor by which to stretch the signal duration */
    double m_stretch_factor;
};

/**
 * @class PitchShiftTransformer
 * @brief Signal transformer that modifies the frequency characteristics of a signal
 *
 * Alters the frequency content of a signal without changing its duration,
 * effectively shifting all frequencies up or down by a specified amount.
 */
class PitchShiftTransformer : public SignalTransformer {
public:
    /**
     * @brief Constructs a pitch shifting transformer
     * @param semitones Number of semitones to shift (positive = higher, negative = lower)
     */
    PitchShiftTransformer(double semitones = 0.0);

    /**
     * @brief Applies pitch shifting to the input signal
     * @param input Container with the signal data to pitch shift
     * @return Container with the pitch-shifted signal data
     */
    virtual std::shared_ptr<Kakshya::SignalSourceContainer> apply_operation(
        std::shared_ptr<Kakshya::SignalSourceContainer> input) override;

private:
    /** @brief Number of semitones to shift the pitch */
    double m_semitones;
};

/**
 * @class SpectralFilterTransformer
 * @brief Signal transformer that selectively attenuates frequency components
 *
 * Applies frequency-domain filtering to a signal, allowing or blocking
 * specific frequency ranges based on the selected filter type and parameters.
 */
class SpectralFilterTransformer : public SignalTransformer {
public:
    /**
     * @enum FilterType
     * @brief Defines the available spectral filtering modes
     */
    enum class FilterType {
        /** @brief Passes frequencies below cutoff */
        LOWPASS,
        /** @brief Passes frequencies above cutoff */
        HIGHPASS,
        /** @brief Passes frequencies between low and high cutoffs */
        BANDPASS,
        /** @brief Blocks frequencies between low and high cutoffs */
        BANDREJECT
    };

    /**
     * @brief Constructs a single-cutoff spectral filter
     * @param type Filter type (LOWPASS or HIGHPASS)
     * @param cutoff_frequency Frequency threshold in Hz
     */
    SpectralFilterTransformer(FilterType type, double cutoff_frequency);

    /**
     * @brief Constructs a dual-cutoff spectral filter
     * @param type Filter type (BANDPASS or BANDREJECT)
     * @param low_cutoff Lower frequency threshold in Hz
     * @param high_cutoff Upper frequency threshold in Hz
     */
    SpectralFilterTransformer(FilterType type, double low_cutoff, double high_cutoff);

    /**
     * @brief Applies spectral filtering to the input signal
     * @param input Container with the signal data to filter
     * @return Container with the filtered signal data
     */
    virtual std::shared_ptr<Kakshya::SignalSourceContainer> apply_operation(
        std::shared_ptr<Kakshya::SignalSourceContainer> input) override;

private:
    /** @brief Type of spectral filter to apply */
    FilterType m_type;
    /** @brief Lower frequency threshold in Hz */
    double m_low_cutoff;
    /** @brief Upper frequency threshold in Hz */
    double m_high_cutoff;
};
}
