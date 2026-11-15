#pragma once

#include "SignatureCollector.hpp"

namespace MayaFlux::Reflect::Tool {

/**
 * @struct SignatureFiles
 * @brief The four generated files of the Creator proxy, as text.
 *
 * includes names the headers the method signatures need, classes names the registered classes'
 * own headers, declarations holds one method declaration per public constructor with its
 * defaults, and definitions holds the matching definitions that build the object and expose it.
 * Type names are relative to MayaFlux, the namespace Creator is declared in, except inside the
 * make_shared call.
 */
struct SignatureFiles {
    std::string includes;
    std::string classes;
    std::string declarations;
    std::string definitions;
};

/**
 * @brief Renders the generated files for the non abstract classes of @p classes.
 * @param source_root The directory MayaFlux/ sits in, which include paths are relative to.
 */
[[nodiscard]] SignatureFiles render_signature_files(
    const std::vector<SignatureClass>& classes, const std::filesystem::path& source_root);

}
