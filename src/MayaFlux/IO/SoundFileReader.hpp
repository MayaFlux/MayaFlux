#pragma once

#include "config.h"
#include "sndfile.hh"

namespace MayaFlux::Containers {
struct RegionPoint;
struct RegionGroup;
class SignalSourceContainer;
}

namespace MayaFlux::IO {

/**
 * @class SoundFileReader
 * @brief Handles reading audio files using libsndfile C++ API
 *
 * SoundFileReader is responsible for loading audio files from disk
 * and transferring the data to a SignalSourceContainer. It encapsulates
 * all interactions with libsndfile, providing a clean interface for
 * loading audio data from various file formats.
 *
 * This implementation uses the libsndfile C++ wrapper (sndfile.hh)
 * and keeps data in interleaved format, letting the container's
 * default processor handle any necessary deinterleaving.
 */
class SoundFileReader {
public:
    /**
     * @brief Creates a new sound file reader
     */
    SoundFileReader();

    /**
     * @brief Destroys the sound file reader
     */
    ~SoundFileReader();

    /**
     * @brief Reads an audio file into a SignalSourceContainer
     * @param file_path Path to the audio file to read
     * @param container Target container to store the audio data
     * @param start_frame Starting frame position for partial loading (default: 0)
     * @param num_frames Number of frames to load, or -1 for all frames (default: -1)
     * @return true if the file was read successfully, false otherwise
     *
     * Loads the audio file at the specified path and transfers the data to the
     * provided container in interleaved format. The container's default processor
     * will handle any necessary deinterleaving.
     */
    bool read_file_to_container(
        const std::string& file_path,
        std::shared_ptr<Containers::SignalSourceContainer> container,
        sf_count_t start_frame = 0,
        sf_count_t num_frames = -1);

    /**
     * @brief Reads an audio file into memory
     * @param file_path Path to the audio file to read
     * @param samples Output vector to store the interleaved sample data
     * @param file_info Output structure to store file metadata
     * @param start_frame Starting frame position for partial loading (default: 0)
     * @param num_frames Number of frames to load, or -1 for all frames (default: -1)
     * @return true if the file was read successfully, false otherwise
     *
     * Loads the audio file at the specified path into memory in interleaved format.
     */
    bool read_file_to_memory(
        const std::string& file_path,
        std::vector<double>& samples,
        SF_INFO& file_info,
        sf_count_t start_frame = 0,
        sf_count_t num_frames = -1);

    /**
     * @brief Gets the most recent error message
     * @return Error message from the last operation, or empty string if no error
     */
    std::string get_last_error() const;

private:
    /**
     * @brief Closes the current file handle
     *
     * Ensures proper cleanup of libsndfile resources.
     */
    void close_file();

    /**
     * @brief Sets the file properties in the provided SF_INFO structure
     * @param file_info Output SF_INFO structure to fill with file metadata
     *
     * Retrieves file metadata from the current sndfile handle and fills
     * the provided structure. Throws std::runtime_error if the handle is invalid.
     */
    void set_file_properties(SF_INFO& file_info);

    /**
     * @brief Extracts marker and region group information from an audio file
     * @param file_path Path to the audio file
     * @param markers Output vector to store extracted markers
     * @param region_groups Output map to store extracted region groups
     * @return true if any metadata was extracted, false otherwise
     *
     * This method examines an audio file for embedded metadata such as cue points,
     * loop points, and broadcast wave format (BWF) information. It organizes this
     * data into categorized region groups for DSP operations and analysis.
     *
     * Supported metadata includes:
     * - Cue points (organized into "cue_points" group)
     * - Loop points (organized into "loops" group)
     * - BWF metadata (organized into "markers" group)
     */
    bool extract_file_metadata(
        const std::string& file_path,
        std::vector<std::pair<std::string, uint64_t>>& markers,
        std::unordered_map<std::string, Containers::RegionGroup>& region_groups);

    /**
     * @brief Extracts marker information from the current file
     * @param markers Output vector to store extracted markers
     * @return true if markers were extracted, false otherwise
     */
    bool extract_markers(std::vector<std::pair<std::string, u_int64_t>>& markers);

    /**
     * @brief Extracts region groups from the current file
     * @param region_groups Output map to store extracted region groups
     * @return true if region groups were extracted, false otherwise
     *
     * This method extracts various types of metadata from the audio file and
     * organizes them into logical groups:
     *
     * - "cue_points": Contains individual cue points from the file
     * - "loops": Contains loop regions with mode and count attributes
     * - "markers": Contains BWF metadata and time references
     *
     * Each group contains RegionPoint objects with start/end frames and
     * relevant attributes specific to the point type.
     */
    bool extract_region_groups(std::unordered_map<std::string, Containers::RegionGroup>& region_groups);

    std::unique_ptr<SndfileHandle> m_sndfile; ///< libsndfile C++ handle
    SF_INFO m_sfinfo; ///< File information
    std::string m_last_error; ///< Last error message
};

} // namespace MayaFlux::IO
