#include "config.h"

namespace MayaFlux::Yantra {

namespace AnalysisConstants {
    // Video/Image analysis
    constexpr double LUMINANCE_WEIGHTS[3] = { 0.299, 0.587, 0.114 }; // RGB to luminance
    constexpr double EDGE_SOBEL_X[9] = { -1, 0, 1, -2, 0, 2, -1, 0, 1 };
    constexpr double EDGE_SOBEL_Y[9] = { -1, -2, -1, 0, 0, 0, 1, 2, 1 };

    // Texture analysis
    constexpr size_t GLCM_LEVELS = 256;
    constexpr double GLCM_ANGLES[4] = { 0.0, std::numbers::pi / 4, std::numbers::pi / 2, 3 * std::numbers::pi / 4 };

    // Multi-modal thresholds
    constexpr double OPTICAL_FLOW_EPSILON = 1e-6;
    constexpr size_t HISTOGRAM_BINS = 256;
}

// ===== Universal Data Type Handling =====

template <typename T>
struct DataTypeTraits {
    static constexpr bool is_integer = std::is_integral_v<T>;
    static constexpr bool is_floating = std::is_floating_point_v<T>;
    static constexpr bool is_complex = false;
    static constexpr double max_value = is_integer ? std::numeric_limits<T>::max() : 1.0;
    static constexpr double min_value = is_integer ? std::numeric_limits<T>::min() : 0.0;
};

template <typename T>
struct DataTypeTraits<std::complex<T>> {
    static constexpr bool is_integer = false;
    static constexpr bool is_floating = true;
    static constexpr bool is_complex = true;
    static constexpr double max_value = 1.0;
    static constexpr double min_value = 0.0;
};

/**
 * @brief Data modality types for cross-modal analysis
 */
enum class DataModality {
    AUDIO_1D, // 1D audio signal
    AUDIO_MULTICHANNEL, // Multi-channel audio
    IMAGE_2D, // 2D image (grayscale or single channel)
    IMAGE_COLOR, // 2D RGB/RGBA image
    VIDEO_GRAYSCALE, // 3D video (time + 2D grayscale)
    VIDEO_COLOR, // 4D video (time + 2D + color)
    TEXTURE_2D, // 2D texture data
    TENSOR_ND, // N-dimensional tensor
    SPECTRAL_2D, // 2D spectral data (time + frequency)
    VOLUMETRIC_3D, // 3D volumetric data
    UNKNOWN
};

}
