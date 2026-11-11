#include "XmlWriter.hpp"

#include "MayaFlux/IO/FileWriter.hpp"

#include <pugixml.hpp>

#include <cctype>
#include <fstream>

namespace MayaFlux::IO {

struct XmlWriter::Impl {
    pugi::xml_document document;
    std::vector<pugi::xml_node> nodes;
};

namespace {

    std::string extension_of(const std::string& filepath)
    {
        auto extension = std::filesystem::path(filepath).extension().string();
        std::ranges::transform(extension, extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return extension;
    }

    bool valid_name(std::string_view name)
    {
        if (name.empty())
            return false;

        const auto first = static_cast<unsigned char>(name.front());
        const bool valid_first = std::isalpha(first) || first == '_'
            || first == ':' || first >= 0x80;

        return valid_first && std::ranges::all_of(name, [](unsigned char c) {
            return std::isalnum(c) || c == '_' || c == ':'
                || c == '-' || c == '.' || c >= 0x80;
        });
    }

    bool valid_xml_value(std::string_view value)
    {
        return std::ranges::all_of(value, [](unsigned char c) {
            return c >= 0x20 || c == '\t' || c == '\n' || c == '\r';
        });
    }

    std::optional<pugi::xml_node_type> type_of(std::string_view kind)
    {
        if (kind == "element")
            return pugi::node_element;

        if (kind == "text")
            return pugi::node_pcdata;

        if (kind == "cdata")
            return pugi::node_cdata;

        if (kind == "comment")
            return pugi::node_comment;

        if (kind == "pi")
            return pugi::node_pi;

        if (kind == "declaration")
            return pugi::node_declaration;

        if (kind == "doctype")
            return pugi::node_doctype;

        return std::nullopt;
    }

}

XmlWriter::XmlWriter() = default;

XmlWriter::~XmlWriter()
{
    if (m_open)
        close();
}

bool XmlWriter::can_write(const std::string& filepath) const
{
    return extension_of(filepath) == ".xml";
}

bool XmlWriter::open(
    const std::string& filepath, const Kakshya::CompositeLayout& layout)
{
    if (m_open) {
        m_last_error = "Close the current XML document before opening another";
        return false;
    }

    m_last_error.clear();
    if (!can_write(filepath)) {
        m_last_error = "Expected an .xml output path";
        return false;
    }

    if (!layout.same_structure(xml_composite_layout())) {
        m_last_error = "XML output requires xml_composite_layout()";
        return false;
    }

    m_xml = std::make_unique<Impl>();
    m_content_started.clear();
    m_ancestors.clear();
    m_filepath = resolve_write_path(filepath);

    std::ofstream output(m_filepath, std::ios::binary | std::ios::trunc);
    if (!output) {
        m_last_error = "Cannot open XML output";
        return false;
    }

    output.close();
    if (!output) {
        m_last_error = "Cannot initialize XML output";
        return false;
    }

    m_failed = false;
    m_declaration_seen = false;
    m_doctype_seen = false;
    m_root_count = 0;
    m_open = true;

    return true;
}

bool XmlWriter::write_rows(const Kakshya::CompositeSlice& rows)
{
    if (!m_open) {
        m_last_error = "No writable XML document is open";
        return false;
    }

    if (m_failed)
        return false;

    if (!rows.layout().same_structure(xml_composite_layout())) {
        m_last_error = "Composite slice layout differs from the XML layout";
        m_failed = true;
        return false;
    }

    for (size_t i = 0; i < rows.size(); ++i) {
        const auto row = rows.at(i);
        if (!row || !write_row(*row)) {
            if (m_last_error.empty())
                m_last_error = "Could not access XML row";

            m_failed = true;
            return false;
        }
    }

    m_last_error.clear();
    return true;
}

bool XmlWriter::write_row(const Kakshya::Composite& row)
{
    const auto kind = row.text("kind");
    const auto name = row.text("name");
    const auto value = row.text("value");
    const auto parent = row.get<uint64_t>("parent");
    const auto index = static_cast<uint64_t>(m_xml->nodes.size());

    if (!kind) {
        m_last_error = "XML row is missing its kind";
        return false;
    }

    if (*kind == "attribute") {
        if (!parent || *parent >= index
            || !name || !valid_name(*name)
            || !value || !valid_xml_value(*value)
            || m_ancestors.empty()
            || m_ancestors.back() != *parent
            || m_content_started[*parent]) {
            m_last_error = "XML attribute requires an open parent and must precede its content";
            return false;
        }

        auto owner = m_xml->nodes[*parent];
        if (owner.type() != pugi::node_element
            && owner.type() != pugi::node_declaration) {
            m_last_error = "XML attribute parent must be an element or declaration";
            return false;
        }

        if (owner.attribute(std::string(*name).c_str())) {
            m_last_error = "XML attribute names must be unique within an element";
            return false;
        }

        auto attribute = owner.append_attribute(std::string(*name).c_str());
        const std::string output_value =
            owner.type() == pugi::node_declaration && *name == "encoding"
                ? "UTF-8"
                : std::string(*value);

        if (!attribute || !attribute.set_value(output_value.c_str())) {
            m_last_error = "Could not append XML attribute";
            return false;
        }

        m_xml->nodes.emplace_back();
        m_content_started.push_back(false);

        return true;
    }

    const auto type = type_of(*kind);
    if (!type) {
        m_last_error = "Unsupported XML node kind";
        return false;
    }

    pugi::xml_node owner = m_xml->document.root();
    if (parent) {
        if (*parent >= index) {
            m_last_error = "XML parent index must precede its child";
            return false;
        }

        while (!m_ancestors.empty() && m_ancestors.back() != *parent)
            m_ancestors.pop_back();

        if (m_ancestors.empty() || m_xml->nodes[*parent].type() != pugi::node_element) {
            m_last_error = "XML parent must be an open ancestor element";
            return false;
        }

        owner = m_xml->nodes[*parent];
        m_content_started[*parent] = true;

        if (*type == pugi::node_declaration || *type == pugi::node_doctype) {
            m_last_error = "XML declaration and doctype must be top-level";
            return false;
        }
    } else {
        m_ancestors.clear();
        if (*type == pugi::node_element) {
            if (m_root_count != 0) {
                m_last_error = "XML document may contain only one root element";
                return false;
            }

            ++m_root_count;
        } else if (*type == pugi::node_declaration) {
            if (m_declaration_seen || !m_xml->nodes.empty()) {
                m_last_error = "XML declaration must be the first document node";
                return false;
            }

            m_declaration_seen = true;
        } else if (*type == pugi::node_doctype) {
            if (m_doctype_seen || m_root_count != 0) {
                m_last_error = "XML doctype must occur once before the root";
                return false;
            }

            m_doctype_seen = true;
        } else if (*type != pugi::node_comment && *type != pugi::node_pi) {
            m_last_error = "XML text and CDATA require a parent element";
            return false;
        }
    }

    const bool needs_name = *type == pugi::node_element
        || *type == pugi::node_pi || *type == pugi::node_declaration;

    const bool needs_value = *type == pugi::node_pcdata
        || *type == pugi::node_cdata || *type == pugi::node_comment
        || *type == pugi::node_pi || *type == pugi::node_doctype;

    if ((needs_name && (!name || !valid_name(*name)))
        || (*type == pugi::node_declaration && (!name || *name != "xml"))
        || (*type == pugi::node_pi && name && *name == "xml")
        || (!needs_name && name)
        || (needs_value && (!value || !valid_xml_value(*value)))
        || (!needs_value && value)
        || (*type == pugi::node_comment
            && (value->find("--") != std::string_view::npos || value->ends_with('-')))
        || (*type == pugi::node_cdata && value->find("]]>") != std::string_view::npos)
        || (*type == pugi::node_pi && value->find("?>") != std::string_view::npos)) {
        m_last_error = "XML row has invalid name or value fields";
        return false;
    }

    auto node = owner.append_child(*type);
    if (!node || (name && !node.set_name(std::string(*name).c_str()))
        || (value && !node.set_value(std::string(*value).c_str()))) {
        m_last_error = "Could not append XML node";
        return false;
    }

    m_xml->nodes.push_back(node);
    m_content_started.push_back(false);

    if (*type == pugi::node_element || *type == pugi::node_declaration)
        m_ancestors.push_back(index);

    return true;
}

bool XmlWriter::flush()
{
    if (!m_open) {
        m_last_error = "No XML document is open";
        return false;
    }

    if (m_failed)
        return false;

    if (m_root_count != 1) {
        m_last_error = "XML document must contain one root element before saving";
        return false;
    }

    const bool saved = m_xml->document.save_file(m_filepath.c_str(), "",
        pugi::format_raw | pugi::format_no_declaration, pugi::encoding_utf8);

    if (!saved) {
        m_last_error = "Could not save XML document";
    } else {
        m_last_error.clear();
    }
    return saved;
}

bool XmlWriter::close()
{
    if (!m_open)
        return true;

    const bool saved = !m_failed && flush();
    m_xml.reset();
    m_content_started.clear();
    m_ancestors.clear();
    m_filepath.clear();
    m_open = false;

    return saved;
}

}
