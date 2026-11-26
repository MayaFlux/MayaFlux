#pragma once

namespace MayaFlux::Kakshya {
class MeshAssemblyContainer;
}

namespace MayaFlux::Nexus::Detail {

/**
 * @brief Read a 3D model or Alembic file as an assembly.
 * @return The assembly, or nullptr with a logged error.
 */
[[nodiscard]] std::shared_ptr<Kakshya::MeshAssemblyContainer> read_assembly(const std::string& filepath);

}
