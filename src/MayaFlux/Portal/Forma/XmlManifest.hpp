#pragma once

#include "Manifest/Instantiate.hpp"

#include "MayaFlux/IO/Composite/XmlDocument.hpp"
#include "MayaFlux/IO/Composite/XmlRecordAccess.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief The Manifest tables for an XML document.
 *
 * Fixes the source type, so a host that loads XML never spells it: every table, binding and
 * registration is written against these aliases. Forma includes no XML header outside this one.
 */
using XmlTables = ManifestTables<IO::XmlElementView>;
using XmlFactoryTable = FactoryTable<IO::XmlElementView>;
using XmlCallTable = CallTable<IO::XmlElementView>;
using XmlActionTable = ActionTable<IO::XmlElementView>;

/**
 * @brief Read an XML file and place it through @p tables.
 *
 * Loads the file, takes its root and runs instantiate_document, so the document, its tables and
 * the surfaces the host bound are all a host supplies. Surfaces stay the host's to create,
 * with create_surface, because that is what binds a window and a name.
 *
 * @param path The XML file.
 * @param tables Factories, calls and actions by name.
 * @param surfaces Surfaces by the name the document's surface elements use.
 * @param error Receives the reason, with a row for a placement error, when nullopt is returned.
 * @return Each placed instance by id, with its surface and element.
 */
[[nodiscard]] inline std::optional<PlacedInstances> instantiate_xml(
    const std::filesystem::path& path, const XmlTables& tables, SurfaceMap& surfaces, std::string& error)
{
    const auto document = IO::XmlDocument::load(path.string(), error);
    if (!document)
        return std::nullopt;
    const auto root = document->root_element();
    if (!root) {
        error = "the document has no root element";
        return std::nullopt;
    }
    return instantiate_document(tables, *root, surfaces, error);
}

}
