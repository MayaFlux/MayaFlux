#pragma once

/**
 * @file WorkflowIncludes.hpp
 * @brief Top level headers for the Yantra compute layer and its workflows.
 *
 * Include after MayaFlux.hpp. Every group is included unless you leave it out
 * by defining its macro before this include:
 * - MAYAFLUX_WORKFLOW_NO_GRANULAR: the granular workflow
 * - MAYAFLUX_COMPUTE_NO_MATRIX: compute matrix, grammar, pipeline and the CPU
 *   analyzers, extractors, sorters and transformers
 * - MAYAFLUX_COMPUTE_NO_GPU: shader execution and the GPU analyzer,
 *   extractor, sorter and transformer
 * - MAYAFLUX_COMPUTE_NO_TEXTURE: texture execution and the vision analyzer
 *   and extractor
 *
 * Only the entry points are listed. Each pulls in the headers it depends on.
 */

#ifndef MAYAFLUX_WORKFLOW_NO_GRANULAR
#include "MayaFlux/Kinesis/Discrete/Taper.hpp"
#include "MayaFlux/Yantra/Workflows/Granular/GranularWorkflow.hpp"
using namespace MayaFlux::Yantra;
#endif

#ifndef MAYAFLUX_COMPUTE_NO_MATRIX
#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/ComputeGrammar.hpp"
#include "MayaFlux/Yantra/ComputeMatrix.hpp"
#include "MayaFlux/Yantra/ComputePipeline.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"
#include "MayaFlux/Yantra/Sorters/StandardSorter.hpp"
#include "MayaFlux/Yantra/Transformers/ConvolutionTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/MathematicalTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/SpectralTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/TemporalTransformer.hpp"
using namespace MayaFlux::Yantra;
#endif

#ifndef MAYAFLUX_COMPUTE_NO_GPU
#include "MayaFlux/Yantra/Analyzers/GpuAnalyzer.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "MayaFlux/Yantra/Extractors/GpuExtractor.hpp"
#include "MayaFlux/Yantra/Sorters/GpuSorter.hpp"
#include "MayaFlux/Yantra/Transformers/GpuTransformer.hpp"
using namespace MayaFlux::Yantra;
#endif

#ifndef MAYAFLUX_COMPUTE_NO_TEXTURE
#include "MayaFlux/Yantra/Analyzers/VisionAnalyzer.hpp"
#include "MayaFlux/Yantra/Executors/TextureExecutionContext.hpp"
#include "MayaFlux/Yantra/Extractors/VisionExtractor.hpp"
using namespace MayaFlux::Yantra;
#endif
