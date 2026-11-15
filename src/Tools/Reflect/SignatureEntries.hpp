#pragma once

namespace MayaFlux::Reflect::Tool {

/**
 * @struct SignatureEntry
 * @brief One class of the Creator registry: the method name, its qualified type and its header.
 */
struct SignatureEntry {
    std::string name;
    std::string type_name;
    std::filesystem::path header;
};

/**
 * @brief Reads the N, W and B registrations of the registry header and finds each class's header.
 *
 * A registration is a line of the form N(Name, MayaFlux::Qualified::Type), with an optional
 * trailing backslash. The header of a class is the one file named after it under
 * @p source_root / MayaFlux that declares it as a class or struct with a body.
 *
 * @param error Receives the reason when nullopt is returned: an unreadable registry, an empty
 * or duplicated registration list, or a class with no header or more than one.
 */
[[nodiscard]] std::optional<std::vector<SignatureEntry>> read_signature_entries(
    const std::filesystem::path& registry, const std::filesystem::path& source_root, std::string& error);

}
