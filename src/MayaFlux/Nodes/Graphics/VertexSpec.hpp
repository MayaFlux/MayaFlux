#pragma once

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

namespace MayaFlux::Nodes {

using PointVertex = Kakshya::PointVertex;
using LineVertex = Kakshya::LineVertex;
using MeshVertex = Kakshya::MeshVertex;
using TextureQuadVertex = Kakshya::TextureQuadVertex;

template <typename T>
Kakshya::VertexLayout vertex_layout_for()
{
    return Kakshya::vertex_layout_for<T>();
}

} // namespace MayaFlux::Nodes
