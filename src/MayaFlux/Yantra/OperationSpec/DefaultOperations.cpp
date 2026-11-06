#include "OperationRegistry.hpp"

#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/VisionAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"
#include "MayaFlux/Yantra/Extractors/VisionExtractor.hpp"
#include "MayaFlux/Yantra/Sorters/StandardSorter.hpp"
#include "MayaFlux/Yantra/Transformers/ConvolutionTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/MathematicalTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/SpectralTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/TemporalTransformer.hpp"

namespace MayaFlux::Yantra {

OperationRegistry make_default_registry()
{
    OperationRegistry registry;

    registry.add<StandardStatisticalAnalyzer>()
        .add<ContainerStatisticalAnalyzer>()
        .add<StandardEnergyAnalyzer>()
        .add<ContainerEnergyAnalyzer>()
        .add<StandardFeatureExtractor>()
        .add<ContainerFeatureExtractor>()
        .add<VisionAnalyzer>()
        .add<VisionExtractor>()
        .add<StandardDataSorter>()
        .add<StandardMatrixSorter>()
        .add<StandardVectorSorterEigen>()
        .add<MathematicalTransformer<>>()
        .add<TemporalTransformer<>>()
        .add<SpectralTransformer<>>()
        .add<ConvolutionTransformer<>>();

    return registry;
}

}
