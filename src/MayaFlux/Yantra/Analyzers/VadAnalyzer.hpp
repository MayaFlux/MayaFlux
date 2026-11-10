#pragma once

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "Analyzer.hpp"
#include <Eigen/Dense>

#include "MayaFlux/Yantra/Executors/Audio/SileroVadModel.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file VadAnalyzer.hpp
 * @brief Voice activity probability per 32 ms chunk from a learned network, run on the GPU.
 */

namespace MayaFlux::Yantra {

/**
 * @struct VadAnalyzerConfig
 * @brief Settings of a VadAnalyzer.
 *
 * The network fixes the chunk at 512 samples of 16 kHz audio, so the window and
 * hop of the base config are both 512 and are not settings.
 */
struct VadAnalyzerConfig : AnalyzerConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            AnalyzerConfig::describe_facets(),
            std::tuple {
                FacetMember<&VadAnalyzerConfig::flush_tail> {},
            });
    }

    VadAnalyzerConfig()
    {
        window_size = SileroVadModel::chunk_samples;
        hop_size = SileroVadModel::chunk_samples;
    }

    /** Also run the partial chunk left at the end of each call, padded with zeros. Off for a stream, on for a whole clip. */
    bool flush_tail { false };
};

/**
 * @struct VadAnalysis
 * @brief Analysis result of a VadAnalyzer.
 */
struct MAYAFLUX_API VadAnalysis {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&VadAnalysis::config> {},
            FacetMember<&VadAnalysis::probabilities> {},
            FacetMember<&VadAnalysis::chunk_positions> {},
        };
    }

    /** Settings the analysis was produced under. */
    VadAnalyzerConfig config;
    /** Speech probability of each chunk completed by this call, 0 to 1. */
    std::vector<float> probabilities;
    /** First and one past the last sample of each chunk, counted from the start of the stream. */
    std::vector<std::pair<size_t, size_t>> chunk_positions;
};

/**
 * @class VadAnalyzer
 * @brief Whether each 32 ms chunk of a mono 16 kHz stream is voice activity, from a learned network.
 *
 * Where WaveformAnalyzer measures the shape of the waveform, this says what
 * the sound is: a sung note and a fan hum are as periodic as a vowel, and only
 * a learned model separates them from speech. The probabilities are the
 * network's own, before any thresholding.
 *
 * The analyzer is named for the task. Today it runs one network, Silero VAD at
 * 16 kHz, recognised by its tensors; weights for any other network are
 * rejected with the reason.
 *
 * The analyzer is a stream. The recurrent state, the 64 samples of context
 * before the next chunk and any partial chunk persist between calls, so
 * successive calls continue one signal. Call reset for a new one. For a whole
 * clip, set flush_tail so the final partial chunk is run too. One channel only.
 *
 * The weights are an explicit dependency: the caller loads the network's
 * tensors and passes them. The network runs in native float on the vulkan
 * target and has no CPU twin. It is not registered with the default registry,
 * because it has no selection to resolve by, and its result arrives a chunk
 * late, so it must not run on the audio thread.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Eigen::MatrixXd>
class VadAnalyzer : public Analyzer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Analyzer<InputType, OutputType>;
    using supports = Supports<VadAnalyzerConfig>;

    static constexpr Target target = targets::vulkan;

    /**
     * @brief Construct from the network weights.
     * @param weights The tensors of the network. Silero VAD at 16 kHz is the one supported, as in silero_vad_16k.safetensors.
     * @param config Analysis settings.
     * @throws std::invalid_argument When the weights are missing a tensor or one has the wrong shape.
     */
    explicit VadAnalyzer(const Kakshya::TensorSet& weights, VadAnalyzerConfig config = {})
    {
        if (const std::string reason = SileroVadModel::diagnose(weights); !reason.empty()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "VadAnalyzer: {}", reason);
        }
        m_weights = std::make_shared<const Kakshya::TensorSet>(weights);
        this->set_config(std::move(config));
    }

    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

    /**
     * @brief Type-safe speech probability analysis
     */
    VadAnalysis analyze_speech(const input_type& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<VadAnalysis>(result);
    }

    VadAnalysis analyze_speech(const InputType& data)
    {
        return this->analyze_speech(input_type { data });
    }

    [[nodiscard]] VadAnalysis get_speech_analysis() const
    {
        return safe_any_cast_or_throw<VadAnalysis>(this->get_current_analysis());
    }

    /**
     * @brief Start a new signal: forget the recurrent state, the context and any partial chunk.
     */
    void reset()
    {
        if (m_model) {
            m_model->reset();
        }
        m_chunks_done = 0;
    }

    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::CUSTOM;
    }

    [[nodiscard]] std::string get_name() const override
    {
        return "VadAnalyzer";
    }

    /**
     * @brief One while a device is up, zero otherwise.
     */
    static float fitness(const input_type& /*input*/)
    {
        return Portal::Graphics::get_shader_foundry().is_initialized() ? 1.0F : 0.0F;
    }

protected:
    output_type analyze_implementation(const input_type& input) override
    {
        try {
            const auto& cfg = this->template config<VadAnalyzerConfig>();

            auto [data_span, structure_info] = OperationHelper::extract_structured_double(
                const_cast<input_type&>(input));

            if (data_span.size() != 1) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "VadAnalyzer analyses one channel and was given {}", data_span.size());
            }

            if (!m_model) {
                m_model = std::make_unique<SileroVadModel>(*m_weights);
            }

            const std::vector<float> samples(data_span[0].begin(), data_span[0].end());

            VadAnalysis analysis;
            analysis.config = cfg;
            analysis.probabilities = m_model->process(samples);
            if (cfg.flush_tail) {
                const auto tail = m_model->flush();
                analysis.probabilities.insert(analysis.probabilities.end(), tail.begin(), tail.end());
            }

            analysis.chunk_positions.reserve(analysis.probabilities.size());
            for (size_t i = 0; i < analysis.probabilities.size(); ++i) {
                const size_t start = (m_chunks_done + i) * SileroVadModel::chunk_samples;
                analysis.chunk_positions.emplace_back(start, start + SileroVadModel::chunk_samples);
            }
            m_chunks_done += analysis.probabilities.size();

            this->store_current_analysis(analysis);

            std::vector<std::vector<double>> channel_values(1);
            channel_values[0].assign(analysis.probabilities.begin(), analysis.probabilities.end());

            output_type output = this->convert_result(channel_values, structure_info);
            output.metadata = input.metadata;
            output.facets = input.facets;
            output.facets.set(analysis);
            return output;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Voice activity analysis failed: {}", e.what());
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    std::shared_ptr<const Kakshya::TensorSet> m_weights;
    mutable std::unique_ptr<SileroVadModel> m_model;
    size_t m_chunks_done { 0 };
};

/// Standard VAD analyzer: DataVariant -> MatrixXd
using StandardVadAnalyzer = VadAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>;

/// Container VAD analyzer: SignalContainer -> MatrixXd
using ContainerVadAnalyzer = VadAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::MatrixXd>;

} // namespace MayaFlux::Yantra
