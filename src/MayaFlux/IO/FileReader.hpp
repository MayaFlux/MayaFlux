#pragma once

#include "config.h"

namespace MayaFlux::IO {

class FileReader {
public:
    virtual ~FileReader() = default;

    virtual bool open(const std::string& path) = 0;
    virtual bool close() = 0;
    virtual bool is_open() const = 0;
    virtual std::string get_read_error() const = 0;
};

// class ReadHelper {
// public:
//     bool read_file_to_container(const std::string& file_path, std::shared_ptr<Kakshya::SignalSourceContainer> container, sf_count_t start_frame = 0, sf_count_t num_frames = -1) = 0;

// };

}
