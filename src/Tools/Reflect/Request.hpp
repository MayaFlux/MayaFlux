#pragma once

namespace MayaFlux::Reflect::Tool {

/**
 * @struct RecordRequest
 * @brief One described record: its qualified name and the names of its public data members.
 *
 * Members of public bases come first, in base declaration order, then the
 * record's own, in declaration order. line is the line after which the sidecar
 * include may appear: the closing brace of the outermost namespace holding the
 * marker, or the marker itself at global scope. The sidecar opens namespaces of
 * its own, so its include must sit at global scope.
 */
struct RecordRequest {
    std::string name;
    std::vector<std::string> members;
    unsigned line {};
};

/**
 * @struct HeaderRequests
 * @brief Every record described by the MF_DESCRIBE markers of one header.
 */
struct HeaderRequests {
    std::filesystem::path header;
    std::vector<RecordRequest> records;
};

using RequestMap = std::map<std::filesystem::path, HeaderRequests>;

}
