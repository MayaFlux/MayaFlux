#include "Creator.hpp"

namespace MayaFlux {

auto CreationProxy::read(const std::string& filepath) -> std::shared_ptr<Kakshya::SoundFileContainer>
{
    auto container = create_container_impl(filepath);
    apply_container_context(container);
    return container;
}

std::shared_ptr<Kakshya::SoundFileContainer> CreationProxy::create_container_impl(const std::string& filepath)
{
    return m_creator->create_container_for_proxy(filepath);
}

void CreationProxy::apply_container_context(std::shared_ptr<Kakshya::SoundFileContainer> container)
{
    if (!container)
        return;
    if (auto& applier = ContextAppliers::get_container_context_applier()) {
        applier(container, m_context);
    }
}

} // namespace MayaFlux
