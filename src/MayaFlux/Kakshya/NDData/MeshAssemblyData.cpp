#include "MeshAssemblyData.hpp"

#include "CompositeDescribe.hpp"

namespace MayaFlux::Kakshya {

CompositeLayout MeshAssemblyData::placement_layout()
{
    return described_composite_layout<MeshAssemblyPlacement>();
}

std::string_view MeshAssemblyData::parent_column()
{
    std::string_view found;
    const auto pick = [&found](const auto& member) {
        using Pointer = std::remove_cvref_t<decltype(member.pointer)>;
        if constexpr (std::same_as<Pointer, decltype(&MeshAssemblyPlacement::parent)>) {
            if (member.pointer == &MeshAssemblyPlacement::parent) {
                found = member.name;
            }
        }
    };
    std::apply([&pick](const auto&... members) { (pick(members), ...); },
        Reflect::Generated::Description<MeshAssemblyPlacement>::members());
    return found;
}

}
