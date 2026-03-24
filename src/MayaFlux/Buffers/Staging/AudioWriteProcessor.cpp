#include "AudioWriteProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/EigenAccess.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

//==============================================================================
// set_data overloads
//==============================================================================

void AudioWriteProcessor::set_data(std::vector<double> data)
{
    m_pending = std::move(data);
    m_dirty.test_and_set(std::memory_order_release);
}

void AudioWriteProcessor::set_data(const Kakshya::DataVariant& variant)
{
    const bool is_glm = std::holds_alternative<std::vector<glm::vec2>>(variant)
        || std::holds_alternative<std::vector<glm::vec3>>(variant)
        || std::holds_alternative<std::vector<glm::vec4>>(variant)
        || std::holds_alternative<std::vector<glm::mat4>>(variant);

    if (is_glm) {
        /** EigenAccess::to_matrix() flattens glm types into a components-as-rows MatrixXd.
        Flatten column-major into a double sequence: [x0,y0,z0, x1,y1,z1, ...]
        **/
        const Eigen::MatrixXd mat = Kakshya::EigenAccess(variant).to_matrix();
        const Eigen::Index total = mat.size();
        m_pending.resize(static_cast<size_t>(total));
        /** Map column-major layout directly — Eigen default is column-major. **/
        Eigen::Map<Eigen::VectorXd>(m_pending.data(), total) = Eigen::Map<const Eigen::VectorXd>(mat.data(), total);
    } else {
        /** Arithmetic and complex types: to_vector() handles magnitude for complex. **/
        const Eigen::VectorXd vec = Kakshya::EigenAccess(variant).to_vector();
        m_pending.resize(static_cast<size_t>(vec.size()));
        Eigen::Map<Eigen::VectorXd>(m_pending.data(), vec.size()) = vec;
    }

    m_dirty.test_and_set(std::memory_order_release);
}

void AudioWriteProcessor::set_data(std::span<const float> data)
{
    m_pending.resize(data.size());
    std::ranges::transform(data, m_pending.begin(),
        [](float s) { return static_cast<double>(s); });
    m_dirty.test_and_set(std::memory_order_release);
}

void AudioWriteProcessor::set_data(std::span<const double> data)
{
    m_pending.assign(data.begin(), data.end());
    m_dirty.test_and_set(std::memory_order_release);
}

void AudioWriteProcessor::clear()
{
    m_pending.clear();
    m_dirty.test_and_set(std::memory_order_release);
}

bool AudioWriteProcessor::has_pending() const noexcept
{
    return m_dirty.test(std::memory_order_acquire);
}

//==============================================================================
// Processor interface
//==============================================================================

void AudioWriteProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (!std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "AudioWriteProcessor requires an AudioBuffer");
    }
}

void AudioWriteProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AudioWriteProcessor attached to non-AudioBuffer");
        return;
    }

    commit_pending();
    write_to_buffer(*audio);
}

bool AudioWriteProcessor::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
}

//==============================================================================
// Private
//==============================================================================

void AudioWriteProcessor::commit_pending()
{
    if (m_dirty.test(std::memory_order_acquire)) {
        m_dirty.clear(std::memory_order_release);
        std::swap(m_active, m_pending);
    }
}

void AudioWriteProcessor::write_to_buffer(AudioBuffer& buf) const
{
    auto& dst = buf.get_data();
    const size_t dst_n = dst.size();

    if (m_active.empty()) {
        std::ranges::fill(dst, 0.0);
        return;
    }

    const size_t copy_n = std::min(dst_n, m_active.size());
    std::ranges::copy_n(m_active.begin(), static_cast<std::ptrdiff_t>(copy_n), dst.begin());

    if (copy_n < dst_n) {
        std::fill(dst.begin() + static_cast<std::ptrdiff_t>(copy_n), dst.end(), 0.0);
    }
}

} // namespace MayaFlux::Buffers
