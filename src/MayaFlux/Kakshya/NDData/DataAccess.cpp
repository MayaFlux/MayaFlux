#include "DataAccess.hpp"

namespace MayaFlux::Kakshya {

std::string DataAccess::type_description() const
{
    if (is_structured()) {
        return std::format("{}×{} ({})",
            element_count(),
            component_count(),
            modality_to_string(m_modality));
    }
    return std::format("scalar×{} ({})",
        element_count(),
        modality_to_string(m_modality));
}

} // namespace MayaFlux::Kakshya
