#include "XmlReader.hpp"

#include "MayaFlux/Kakshya/NDData/CompositeInsertion.hpp"

#include <cstddef>
#include <pugixml.hpp>

#include <cctype>

namespace MayaFlux::IO {

Kakshya::CompositeLayout xml_composite_layout()
{
    Kakshya::CompositeLayout layout;
    layout.add_field<uint64_t>("parent");
    layout.add_field<std::string>("kind");
    layout.add_field<std::string>("name");
    layout.add_field<std::string>("value");

    return layout;
}

std::string_view XmlElementView::name() const noexcept
{
    const auto source = m_rows->at(m_row);
    return source ? source->text("name").value_or(std::string_view {}) : std::string_view {};
}

std::optional<std::string_view> XmlElementView::attribute(std::string_view name) const noexcept
{
    for (size_t index = m_row + 1; index < (*m_subtree_end)[m_row]; ++index) {
        const auto source = m_rows->at(index);
        if (!source || source->text("kind") != std::string_view("attribute"))
            break;
        if (source->text("name") == name)
            return source->text("value");
    }
    return std::nullopt;
}

namespace {

    constexpr uintmax_t max_xml_file_bytes = static_cast<const uintmax_t>(64 * 1024 * 1024);
    constexpr size_t max_xml_nodes = 1'000'000;
    constexpr size_t max_xml_depth = 256;

    std::string extension_of(const std::string& filepath)
    {
        auto extension = std::filesystem::path(filepath).extension().string();
        std::ranges::transform(extension, extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        return extension;
    }

    std::string_view node_kind(pugi::xml_node_type type)
    {
        switch (type) {
        case pugi::node_element:
            return "element";
        case pugi::node_pcdata:
            return "text";
        case pugi::node_cdata:
            return "cdata";
        case pugi::node_comment:
            return "comment";
        case pugi::node_pi:
            return "pi";
        case pugi::node_declaration:
            return "declaration";
        case pugi::node_doctype:
            return "doctype";
        default:
            return {};
        }
    }

    struct PendingNode {
        pugi::xml_node node;
        std::optional<uint64_t> parent;
        size_t depth;
    };

    void push_children(std::vector<PendingNode>& pending,
        const pugi::xml_node& node, std::optional<uint64_t> parent, size_t depth)
    {
        for (auto child = node.last_child(); child; child = child.previous_sibling())
            pending.push_back({ .node = child, .parent = parent, .depth = depth });
    }

}

bool XmlReader::can_read(const std::string& filepath) const
{
    return extension_of(filepath) == ".xml";
}

bool XmlReader::open(const std::string& filepath, FileReadOptions)
{
    close();
    m_last_error.clear();

    if (!can_read(filepath)) {
        m_last_error = "Expected an .xml file";
        return false;
    }

    m_filepath = resolve_path(filepath);
    std::error_code error;
    const auto file_size = std::filesystem::file_size(m_filepath, error);
    if (error) {
        m_last_error = "Cannot open XML file";
        return false;
    }

    if (file_size > max_xml_file_bytes) {
        m_last_error = "XML file exceeds the 64 MiB input limit";
        return false;
    }

    pugi::xml_document document;
    const auto parsed = document.load_file(m_filepath.c_str(),
        pugi::parse_full | pugi::parse_ws_pcdata);
    if (!parsed) {
        m_last_error = "XML parse failed at byte "
            + std::to_string(parsed.offset) + ": " + parsed.description();
        return false;
    }

    if (!document.document_element()) {
        m_last_error = "XML document has no root element";
        return false;
    }

    Kakshya::CompositeArray rows(xml_composite_layout());
    Kakshya::CompositeInsertion insertion(rows);
    std::vector<PendingNode> pending;
    push_children(pending, document, std::nullopt, 0);

    while (!pending.empty()) {
        auto entry = pending.back();
        pending.pop_back();

        if (!entry.parent && entry.node.type() == pugi::node_pcdata) {
            const std::string_view value(entry.node.value());

            if (std::ranges::all_of(value, [](unsigned char c) {
                    return std::isspace(c) != 0;
                }))
                continue;
        }

        if (entry.depth > max_xml_depth || rows.size() >= max_xml_nodes) {
            m_last_error = "XML document exceeds the node or depth limit";
            return false;
        }

        const auto kind = node_kind(entry.node.type());
        if (kind.empty()) {
            m_last_error = "Unsupported XML node type";
            return false;
        }

        const auto row = insertion.append();
        if (!insertion.set_text(row, "kind", kind)
            || (entry.parent && !insertion.set(row, "parent", *entry.parent))) {
            m_last_error = "Could not store XML node";
            return false;
        }

        const std::string_view name(entry.node.name());
        const std::string_view value(entry.node.value());
        const bool has_value = entry.node.type() == pugi::node_pcdata
            || entry.node.type() == pugi::node_cdata
            || entry.node.type() == pugi::node_comment
            || entry.node.type() == pugi::node_pi
            || entry.node.type() == pugi::node_doctype;

        if ((!name.empty() && !insertion.set_text(row, "name", name))
            || (has_value && !insertion.set_text(row, "value", value))) {
            m_last_error = "Could not store XML node content";
            return false;
        }

        for (auto attribute : entry.node.attributes()) {
            if (rows.size() >= max_xml_nodes) {
                m_last_error = "XML document exceeds the node limit";
                return false;
            }

            const auto attribute_row = insertion.append();
            if (!insertion.set(attribute_row, "parent", static_cast<uint64_t>(row))
                || !insertion.set_text(attribute_row, "kind", "attribute")
                || !insertion.set_text(attribute_row, "name", attribute.name())
                || !insertion.set_text(attribute_row, "value", attribute.value())) {
                m_last_error = "Could not store XML attribute";
                return false;
            }
        }

        push_children(pending, entry.node, static_cast<uint64_t>(row), entry.depth + 1);
    }

    std::vector<size_t> subtree_end(rows.size(), rows.size());
    std::vector<size_t> ancestry;
    ancestry.reserve(max_xml_depth + 2);
    for (size_t index = 0; index < rows.size(); ++index) {
        const auto source = rows.at(index);
        const auto parent = source->get<uint64_t>("parent");
        while (!ancestry.empty() && (!parent || ancestry.back() != *parent)) {
            subtree_end[ancestry.back()] = index;
            ancestry.pop_back();
        }
        ancestry.push_back(index);
    }
    for (const size_t index : ancestry)
        subtree_end[index] = rows.size();

    m_subtree_end = std::move(subtree_end);
    m_rows.emplace(std::move(rows));
    m_position = 0;

    return true;
}

void XmlReader::close()
{
    m_rows.reset();
    m_subtree_end.clear();
    m_filepath.clear();
    m_position = 0;
}

std::optional<FileMetadata> XmlReader::get_metadata() const
{
    if (!m_rows)
        return std::nullopt;

    FileMetadata metadata;
    metadata.format = "xml";
    metadata.mime_type = "application/xml";

    std::error_code error;
    metadata.file_size = std::filesystem::file_size(m_filepath, error);
    if (error)
        metadata.file_size = 0;

    return metadata;
}

std::optional<Kakshya::CompositeArray> XmlReader::read_composite()
{
    return read_next(std::numeric_limits<size_t>::max());
}

std::optional<Kakshya::CompositeArray>
XmlReader::read_next(size_t max_elements)
{
    if (!m_rows) {
        m_last_error = "No XML document is open";
        return std::nullopt;
    }

    Kakshya::CompositeArray result(m_rows->layout());
    Kakshya::CompositeInsertion insertion(result);
    const auto count = std::min(max_elements,
        m_rows->size() - static_cast<size_t>(m_position));

    for (size_t i = 0; i < count; ++i) {
        const auto source = m_rows->at(static_cast<size_t>(m_position) + i);
        if (!source || !insertion.append(*source)) {
            m_last_error = "Could not copy XML node";
            return std::nullopt;
        }
    }

    m_position += count;
    m_last_error.clear();
    return result;
}

std::optional<Kakshya::CompositeLayout> XmlReader::get_layout() const
{
    if (!m_rows)
        return std::nullopt;

    return m_rows->layout();
}

std::optional<XmlElementView> XmlReader::root_element() const noexcept
{
    if (!m_rows)
        return std::nullopt;

    for (size_t index = 0; index < m_rows->size(); index = m_subtree_end[index]) {
        const auto source = m_rows->at(index);
        if (source && source->text("kind") == std::string_view("element"))
            return XmlElementView(*m_rows, m_subtree_end, index);
    }
    return std::nullopt;
}

std::optional<XmlElementView> XmlReader::element_at(uint64_t row) const noexcept
{
    if (!m_rows || row >= m_rows->size())
        return std::nullopt;

    const auto source = m_rows->at(static_cast<size_t>(row));
    if (!source || source->text("kind") != std::string_view("element"))
        return std::nullopt;

    return XmlElementView(*m_rows, m_subtree_end, static_cast<size_t>(row));
}

bool XmlReader::seek(const std::vector<uint64_t>& position)
{
    if (!m_rows || position.size() != 1 || position[0] > m_rows->size()) {
        m_last_error = "Seek requires a valid XML node position";
        return false;
    }

    m_position = position[0];
    m_last_error.clear();
    return true;
}

std::vector<uint64_t> XmlReader::get_dimension_sizes() const
{
    return m_rows ? std::vector<uint64_t> { m_rows->size() }
                  : std::vector<uint64_t> {};
}

}
