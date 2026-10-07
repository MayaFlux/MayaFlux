#pragma once

/**
 * @file SimulationIncludes.hpp
 * @brief Headers for field, grid, volume and spatial cache simulations.
 *
 * Include after MayaFlux.hpp. Every group is included unless you leave it out
 * by defining its macro before this include:
 * - MAYAFLUX_SIMULATION_NO_FIELDS: GPU field operators, vertex and UV field
 *   processors, spatial hash, claim and population processors, force fields,
 *   and the spatial cache, transfer, reader and export
 * - MAYAFLUX_SIMULATION_NO_GRID: relaxation grid buffers and processors
 * - MAYAFLUX_SIMULATION_NO_VOLUME: volume buffers, fluid processors,
 *   raymarching and volume file IO
 *
 * Code that builds directly on the underlying types can include only the
 * headers it uses instead.
 */

#ifndef MAYAFLUX_SIMULATION_NO_FIELDS
#include "Buffers/Network/MutationClaimProcessor.hpp"
#include "Buffers/Network/NetworkGeometryProcessor.hpp"
#include "Buffers/Network/PopulationProcessor.hpp"
#include "Buffers/Network/SpatialHashProcessor.hpp"
#include "Buffers/Shaders/UVFieldProcessor.hpp"
#include "Buffers/Shaders/VertexFieldProcessor.hpp"
#include "IO/Spatial/SpatialCache.hpp"
#include "IO/Spatial/SpatialExport.hpp"
#include "IO/Spatial/SpatialReader.hpp"
#include "IO/Spatial/SpatialTransfer.hpp"
#include "Kinesis/Tendency/DualField.hpp"
#include "Kinesis/Tendency/FieldBinding.hpp"
#include "Kinesis/Tendency/ForceFields.hpp"
#include "Nodes/Network/Operators/FieldOperator.hpp"
#include "Nodes/Network/Operators/GpuFieldOperator.hpp"
#include "Nodes/Network/Operators/InstanceFieldOperator.hpp"
#include "Nodes/Network/Operators/MeshFieldOperator.hpp"
#endif

#ifndef MAYAFLUX_SIMULATION_NO_GRID
#include "Buffers/State/RelaxationEmitProcessor.hpp"
#include "Buffers/State/RelaxationGridBuffer.hpp"
#include "Buffers/State/RelaxationStepProcessor.hpp"
#endif

#ifndef MAYAFLUX_SIMULATION_NO_VOLUME
#include "Buffers/Shaders/SDFMeshProcessor.hpp"
#include "Buffers/State/AdvectProcessor.hpp"
#include "Buffers/State/BuoyancyProcessor.hpp"
#include "Buffers/State/DiffuseProcessor.hpp"
#include "Buffers/State/DivergenceProcessor.hpp"
#include "Buffers/State/InfluxProcessor.hpp"
#include "Buffers/State/PressureProcessor.hpp"
#include "Buffers/State/RaymarchBuffer.hpp"
#include "Buffers/State/RaymarchProcessor.hpp"
#include "Buffers/State/SolenoidalProcessor.hpp"
#include "Buffers/State/VolumeFieldProcessor.hpp"
#include "Buffers/State/VolumeGridBuffer.hpp"
#include "Buffers/State/VolumeSurfaceProcessor.hpp"
#include "Buffers/State/WallProcessor.hpp"
#include "IO/Volume/VolumeReader.hpp"
#include "IO/Volume/VolumeTransfer.hpp"
#include "IO/Volume/VolumeWriter.hpp"
#endif
