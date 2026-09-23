#include "DelimitedTextReader.hpp"

#include "MayaFlux/Kakshya/NDData/CompositeInsertion.hpp"

namespace MayaFlux::IO {

namespace {

    std::string extension_of(const std::string& filepath)
    {
        auto extension = std::filesystem::path(filepath).extension().string();

        std::ranges::transform(extension, extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        return extension;
    }

    std::string_view trim_numeric(std::string_view value)
    {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
            value.remove_prefix(1);

        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
            value.remove_suffix(1);

        return value;
    }

    template <typename T>
    bool parse_number(std::string_view text, T& value)
    {
        text = trim_numeric(text);
        if (text.empty())
            return false;

        if constexpr (std::same_as<T, bool>) {
            if (text == "true" || text == "1") {
                value = true;
                return true;
            }
            if (text == "false" || text == "0") {
                value = false;
                return true;
            }

            return false;
        } else if constexpr (std::integral<T>) {
            using Parsed = std::conditional_t<std::is_signed_v<T>, long long, unsigned long long>;
            Parsed parsed {};

            const auto [end, error] = std::from_chars(
                text.data(), text.data() + text.size(), parsed);

            if (error != std::errc {} || end != text.data() + text.size()
                || parsed < static_cast<Parsed>(std::numeric_limits<T>::lowest())
                || parsed > static_cast<Parsed>(std::numeric_limits<T>::max()))
                return false;

            value = static_cast<T>(parsed);
            return true;
        } else {
            const auto [end, error] = std::from_chars(
                text.data(), text.data() + text.size(), value);

            return error == std::errc {} && end == text.data() + text.size()
                && std::isfinite(value);
        }
    }

    template <typename T>
    bool set_numeric(Kakshya::CompositeInsertion& insertion, size_t row,
        std::string_view name, std::string_view text)
    {
        T value {};

        return parse_number(text, value) && insertion.set(row, name, value);
    }

    bool supported_field(const Kakshya::CompositeField& field)
    {
        const auto type = field.type;

        return type == typeid(std::string)
            || type == typeid(bool)
            || type == typeid(char)
            || type == typeid(signed char)
            || type == typeid(unsigned char)
            || type == typeid(short)
            || type == typeid(unsigned short)
            || type == typeid(int)
            || type == typeid(unsigned int)
            || type == typeid(long)
            || type == typeid(unsigned long)
            || type == typeid(long long)
            || type == typeid(unsigned long long)
            || type == typeid(float)
            || type == typeid(double)
            || type == typeid(long double);
    }

    bool set_field(Kakshya::CompositeInsertion& insertion, size_t row,
        const Kakshya::CompositeField& field, std::string_view text)
    {
        const auto name = std::string_view(field.name);
        const auto type = field.type;

        if (type == typeid(std::string))
            return insertion.set_text(row, name, text);

        if (trim_numeric(text).empty())
            return true;

        if (type == typeid(bool))
            return set_numeric<bool>(insertion, row, name, text);

        if (type == typeid(char))
            return set_numeric<char>(insertion, row, name, text);
        if (type == typeid(signed char))
            return set_numeric<signed char>(insertion, row, name, text);
        if (type == typeid(unsigned char))
            return set_numeric<unsigned char>(insertion, row, name, text);

        if (type == typeid(short))
            return set_numeric<short>(insertion, row, name, text);
        if (type == typeid(unsigned short))
            return set_numeric<unsigned short>(insertion, row, name, text);

        if (type == typeid(int))
            return set_numeric<int>(insertion, row, name, text);
        if (type == typeid(unsigned int))
            return set_numeric<unsigned int>(insertion, row, name, text);

        if (type == typeid(long))
            return set_numeric<long>(insertion, row, name, text);
        if (type == typeid(unsigned long))
            return set_numeric<unsigned long>(insertion, row, name, text);

        if (type == typeid(long long))
            return set_numeric<long long>(insertion, row, name, text);
        if (type == typeid(unsigned long long))
            return set_numeric<unsigned long long>(insertion, row, name, text);

        if (type == typeid(float))
            return set_numeric<float>(insertion, row, name, text);
        if (type == typeid(double))
            return set_numeric<double>(insertion, row, name, text);
        if (type == typeid(long double))
            return set_numeric<long double>(insertion, row, name, text);

        return false;
    }

}

DelimitedTextReader::~DelimitedTextReader() = default;

bool DelimitedTextReader::set_layout(Kakshya::CompositeLayout layout)
{
    if (is_open())
        return false;

    m_requested_layout = std::move(layout);
    return true;
}

bool DelimitedTextReader::set_has_header(bool enabled) noexcept
{
    if (is_open())
        return false;

    m_has_header = enabled;
    return true;
}

bool DelimitedTextReader::set_delimiter(char delimiter) noexcept
{
    if (is_open() || delimiter == '\0' || delimiter == '"'
        || delimiter == '\r' || delimiter == '\n')
        return false;

    m_delimiter = delimiter;
    return true;
}

bool DelimitedTextReader::can_read(const std::string& filepath) const
{
    const auto extension = extension_of(filepath);
    return extension == ".csv" || extension == ".tsv";
}

bool DelimitedTextReader::open(
    const std::string& filepath, FileReadOptions)
{
    close();
    m_last_error.clear();
    if (!can_read(filepath)) {
        m_last_error = "Expected a .csv or .tsv file";
        return false;
    }

    if (!m_has_header && !m_requested_layout) {
        m_last_error = "Headerless input requires a supplied CompositeLayout";
        return false;
    }

    m_filepath = resolve_path(filepath);
    m_active_delimiter = m_delimiter != '\0'
        ? m_delimiter
        : (extension_of(filepath) == ".tsv" ? '\t' : ',');

    m_file.open(m_filepath, std::ios::binary);
    if (!m_file) {
        m_last_error = "Cannot open delimited text file";
        return false;
    }

    std::vector<std::string> names;
    bool at_end = false;
    if (m_has_header) {
        if (!read_record(names, at_end) || at_end) {
            if (at_end)
                m_last_error = "Delimited text file has no header";
            close();
            return false;
        }

        if (!names.empty() && names.front().starts_with("\xEF\xBB\xBF"))
            names.front().erase(0, 3);
    }

    if (m_requested_layout) {
        m_layout = *m_requested_layout;
        const auto& fields = m_layout->fields();
        if (fields.empty() || (m_has_header && fields.size() != names.size())) {
            m_last_error = "Header does not match the supplied CompositeLayout";
            close();
            return false;
        }

        for (size_t i = 0; i < fields.size(); ++i) {
            if (!supported_field(fields[i])
                || (m_has_header && fields[i].name != names[i])) {
                m_last_error = "Unsupported or mismatched Composite field: " + fields[i].name;
                close();
                return false;
            }
        }
    } else {
        Kakshya::CompositeLayout layout;
        for (auto& name : names) {
            if (!layout.add_field<std::string>(std::move(name))) {
                m_last_error = "Header field names must be nonempty and unique";
                close();
                return false;
            }
        }

        m_layout = std::move(layout);
    }

    Kakshya::CompositeArray empty(*m_layout);
    m_layout = empty.layout();

    m_data_start = m_file.tellg();
    if (m_data_start == std::streampos(-1)) {
        m_file.clear();
        m_file.seekg(0, std::ios::end);
        m_data_start = m_file.tellg();
    }

    m_file.clear();
    m_file.seekg(m_data_start);

    m_row_position = 0;
    m_at_end = false;
    return true;
}

void DelimitedTextReader::close()
{
    if (m_file.is_open())
        m_file.close();

    m_layout.reset();
    m_filepath.clear();
    m_row_position = 0;
    m_at_end = false;
}

std::optional<FileMetadata> DelimitedTextReader::get_metadata() const
{
    if (!is_open() || !m_layout)
        return std::nullopt;

    FileMetadata metadata;
    metadata.format = m_active_delimiter == '\t' ? "tsv" : "csv";
    metadata.mime_type = m_active_delimiter == '\t'
        ? "text/tab-separated-values"
        : "text/csv";

    std::error_code error;
    metadata.file_size = std::filesystem::file_size(m_filepath, error);
    if (error)
        metadata.file_size = 0;

    std::vector<std::string> names;
    names.reserve(m_layout->fields().size());
    for (const auto& field : m_layout->fields())
        names.push_back(field.name);

    metadata.attributes.emplace("fields", std::move(names));
    return metadata;
}

std::optional<Kakshya::CompositeArray>
DelimitedTextReader::read_composite()
{
    return read_next(std::numeric_limits<size_t>::max());
}

std::optional<Kakshya::CompositeArray>
DelimitedTextReader::read_next(size_t max_elements)
{
    if (!is_open() || !m_layout) {
        m_last_error = "No delimited text file is open";
        return std::nullopt;
    }

    Kakshya::CompositeArray result(*m_layout);
    std::vector<std::string> fields;

    while (result.size() < max_elements && !m_at_end) {
        bool at_end = false;

        if (!read_record(fields, at_end))
            return std::nullopt;

        if (at_end) {
            m_at_end = true;
            break;
        }

        if (!append_record(result, fields))
            return std::nullopt;

        ++m_row_position;
    }

    m_last_error.clear();
    return result;
}

bool DelimitedTextReader::seek(
    const std::vector<uint64_t>& position)
{
    if (!is_open() || position.size() != 1) {
        m_last_error = "Seek requires one record position in an open file";
        return false;
    }
    m_file.clear();
    m_file.seekg(m_data_start);
    m_row_position = 0;
    m_at_end = false;

    std::vector<std::string> fields;

    while (m_row_position < position[0]) {
        bool at_end = false;

        if (!read_record(fields, at_end))
            return false;

        if (at_end) {
            m_at_end = true;
            m_last_error = "Seek position is past the end of the file";
            return false;
        }

        ++m_row_position;
    }

    m_last_error.clear();
    return true;
}

bool DelimitedTextReader::read_record(
    std::vector<std::string>& fields, bool& at_end)
{
    fields.clear();
    at_end = false;

    std::string value;
    bool quoted = false;
    bool closed_quote = false;
    bool field_start = true;
    bool saw_input = false;

    while (true) {
        const int next = m_file.get();
        if (next == std::char_traits<char>::eof()) {
            if (m_file.bad()) {
                m_last_error = "Failed to read delimited text file";
                return false;
            }

            if (quoted) {
                m_last_error = "Unterminated quoted field";
                return false;
            }

            if (!saw_input) {
                at_end = true;
                return true;
            }

            fields.push_back(std::move(value));
            return true;
        }

        saw_input = true;
        const char c = static_cast<char>(next);
        if (quoted) {
            if (c == '"') {
                if (m_file.peek() == '"') {
                    m_file.get();
                    value.push_back('"');
                } else {
                    quoted = false;
                    closed_quote = true;
                }
            } else {
                value.push_back(c);
            }

            continue;
        }

        if (c == m_active_delimiter || c == '\n' || c == '\r') {
            fields.push_back(std::move(value));
            value.clear();
            field_start = true;
            closed_quote = false;
            if (c == '\r' && m_file.peek() == '\n')
                m_file.get();

            if (c != m_active_delimiter)
                return true;

            continue;
        }

        if (c == '"' && field_start) {
            quoted = true;
            field_start = false;
            continue;
        }

        if (c == '"' || closed_quote) {
            m_last_error = "Unexpected character after quoted field";
            return false;
        }

        value.push_back(c);
        field_start = false;
    }
}

bool DelimitedTextReader::append_record(
    Kakshya::CompositeArray& array, const std::vector<std::string>& fields)
{
    const auto& layout_fields = array.layout().fields();
    if (fields.size() > layout_fields.size()) {
        m_last_error = "Record has more fields than the CompositeLayout";
        return false;
    }

    Kakshya::CompositeInsertion insertion(array);
    const auto row = insertion.append();

    for (size_t i = 0; i < fields.size(); ++i) {
        if (!set_field(insertion, row, layout_fields[i], fields[i])) {
            m_last_error = "Invalid value in field '" + layout_fields[i].name
                + "' at record " + std::to_string(m_row_position);
            return false;
        }
    }

    return true;
}

}
