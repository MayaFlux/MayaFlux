#include "TypeFaceFoundry.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Text {

bool TypeFaceFoundry::initialize()
{
    if (m_library) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "TypeFaceFoundry already initialized");
        return true;
    }

    if (const FT_Error err = FT_Init_FreeType(&m_library); err != 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "FT_Init_FreeType failed with error {}", static_cast<int>(err));
        m_library = nullptr;
        return false;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "TypeFaceFoundry initialized");
    return true;
}

void TypeFaceFoundry::shutdown()
{
    if (!m_library) {
        return;
    }

    FT_Done_FreeType(m_library);
    m_library = nullptr;

    MF_INFO(Journal::Component::Portal, Journal::Context::API,
        "TypeFaceFoundry shutdown");
}

} // namespace MayaFlux::Portal::Text
