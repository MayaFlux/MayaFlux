#include "JSONSerializer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <fstream>

namespace MayaFlux::IO {

std::string JSONSerializer::encode(const nlohmann::json& doc, int indent)
{
    return doc.dump(indent);
}

std::optional<nlohmann::json> JSONSerializer::decode(const std::string& str)
{
    m_last_error.clear();

    try {
        return nlohmann::json::parse(str);
    } catch (const nlohmann::json::parse_error& e) {
        m_last_error = std::string("JSON parse error: ") + e.what();
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return std::nullopt;
    }
}

bool JSONSerializer::write(const std::string& path, const nlohmann::json& doc, int indent)
{
    m_last_error.clear();

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        m_last_error = "Failed to open file for writing: " + path;
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    file << encode(doc, indent);
    if (!file.good()) {
        m_last_error = "Write failed: " + path;
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    return true;
}

std::optional<nlohmann::json> JSONSerializer::read(const std::string& path)
{
    m_last_error.clear();

    std::ifstream file(path);
    if (!file.is_open()) {
        m_last_error = "Failed to open file for reading: " + path;
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return std::nullopt;
    }

    try {
        return nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error& e) {
        m_last_error = std::string("JSON parse error in ") + path + ": " + e.what();
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return std::nullopt;
    }
}

} // namespace MayaFlux::IO
