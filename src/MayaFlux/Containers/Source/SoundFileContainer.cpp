#include "SoundFileContainer.hpp"

#include "MayaFlux/Buffers/Container/ContainerBuffer.hpp"
#include "MayaFlux/Containers/Processors/ContiguousAccessProcessor.hpp"

namespace MayaFlux::Containers {

SoundFileContainer::SoundFileContainer()
    : m_ready_for_processing(false)
    , m_looping(false)
    , m_read_position(0)
    , m_processing_state(ProcessingState::IDLE)
    , m_num_channels(1)
{
}

void SoundFileContainer::create_default_processor()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    try {
        m_default_processor = std::make_shared<ContiguousAccessProcessor>();

        if (m_default_processor && !m_samples.empty()) {
            try {
                m_default_processor->on_attach(shared_from_this());
                m_ready_for_processing = true;
                update_processing_state(ProcessingState::READY);
            } catch (const std::bad_weak_ptr& e) {
                std::cerr << "Cannot attach processor - container not owned by shared_ptr yet" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in create_default_processor: " << e.what() << std::endl;
    }
}

std::string SoundFileContainer::get_file_path() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_file_path;
}

void SoundFileContainer::setup(u_int32_t num_frames, u_int32_t sample_rate, u_int32_t num_channels)
{
    m_num_frames = num_frames;
    m_sample_rate = sample_rate;
    m_num_channels = num_channels;

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    m_processed_data.resize(m_num_channels);

    for (auto& channel : m_processed_data) {
        channel.resize(num_frames, 0.0);
    }

    m_buffers.clear();
}

void SoundFileContainer::create_container_buffers()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_buffers.empty() && !m_samples.empty()) {
        try {
            auto self = shared_from_this();

            m_buffers.reserve(m_num_channels);

            for (size_t channel = 0; channel < m_num_channels; ++channel) {
                try {
                    auto buffer = std::make_shared<Buffers::ContainerBuffer>(
                        static_cast<u_int32_t>(channel),
                        512,
                        self,
                        static_cast<u_int32_t>(channel));

                    if (buffer) {
                        buffer->initialize();
                        m_buffers.push_back(std::move(buffer));
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Exception creating buffer for channel " << channel
                              << ": " << e.what() << std::endl;
                }
            }
        } catch (const std::bad_weak_ptr& e) {
            std::cerr << "Cannot create buffers - container not owned by shared_ptr" << std::endl;
            std::cerr << "Exception details: " << e.what() << std::endl;
        }
    }
}

void SoundFileContainer::resize(u_int32_t num_frames)
{

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (auto& channel : m_processed_data) {
        channel.resize(num_frames, 0.0);
    }
}

void SoundFileContainer::clear()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (auto& channel : m_processed_data) {
        std::fill(channel.begin(), channel.end(), 0.0);
    }

    for (auto& samples : m_samples) {
        std::fill(samples.begin(), samples.end(), 0.0);
    }

    m_read_position = 0;
}

u_int32_t SoundFileContainer::get_num_frames() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_samples.empty()) {
        return 0;
    }

    return static_cast<u_int32_t>(m_samples[0].size());
}

double SoundFileContainer::get_sample_at(u_int64_t sample_index, u_int32_t channel) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_samples.empty() || channel >= m_num_channels || sample_index >= m_num_frames || m_processing_state < ProcessingState::READY || m_processing_state == ProcessingState::NEEDS_REMOVAL) {
        return 0.0;
    }

    if (is_interleaved() && !m_samples.empty()) {
        u_int64_t interleaved_index = sample_index * m_num_channels + channel;
        if (interleaved_index < m_samples[0].size()) {
            return m_samples[0][interleaved_index];
        }
    } else if (channel < m_samples.size() && sample_index < m_samples[channel].size()) {
        return m_samples[channel][sample_index];
    }
}

std::vector<double> SoundFileContainer::get_frame_at(u_int64_t frame_index) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    std::vector<double> frame;
    if (m_samples.empty() || frame_index >= m_num_frames) {
        return frame;
    }

    frame.reserve(m_num_channels);
    for (auto samples : m_samples) {
        frame.push_back(samples[frame_index]);
    }

    return frame;
}

const bool SoundFileContainer::is_ready_for_processing() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_ready_for_processing;
}

void SoundFileContainer::mark_ready_for_processing(bool ready)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_ready_for_processing = ready;

    if (ready && m_processing_state == ProcessingState::IDLE) {
        update_processing_state(ProcessingState::READY);
    }
}

void SoundFileContainer::fill_sample_range(u_int64_t start, u_int32_t num_samples, std::vector<double>& output_buffer, u_int32_t channel) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (channel >= m_samples.size() || m_samples[channel].empty()) {
        output_buffer.assign(num_samples, 0.0);
        return;
    }

    if (output_buffer.size() < num_samples) {
        output_buffer.resize(num_samples);
    }

    const auto& channel_data = m_samples[channel];
    const u_int64_t channel_size = channel_data.size();

    for (u_int32_t i = 0; i < num_samples; ++i) {
        u_int64_t pos = start + i;

        if (pos < channel_size) {
            output_buffer[i] = channel_data[pos];
        } else if (m_looping && channel_size > 0) {
            output_buffer[i] = channel_data[pos % channel_size];
        } else {
            output_buffer[i] = 0.0;
        }
    }
}

void SoundFileContainer::fill_frame_range(u_int64_t start_frame, u_int32_t num_frames, std::vector<std::vector<double>>& output_buffers, const std::vector<u_int32_t>& channels) const
{
}

bool SoundFileContainer::is_range_valid(u_int64_t start_frame, u_int32_t num_frames) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_samples.empty() || m_samples[0].empty()) {
        return false;
    }

    const u_int64_t channel_size = m_samples[0].size();
    return start_frame < channel_size && (start_frame + num_frames) <= channel_size;
}

void SoundFileContainer::set_read_position(u_int64_t frame_position)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_samples.empty() || m_samples[0].empty()) {
        m_read_position = 0;
        return;
    }

    const u_int64_t channel_size = m_samples[0].size();

    if (frame_position < channel_size) {
        m_read_position = frame_position;
    } else if (m_looping && channel_size > 0) {
        m_read_position = frame_position % channel_size;
    } else {
        m_read_position = channel_size;
    }
}

u_int64_t SoundFileContainer::get_read_position() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_read_position;
}

void SoundFileContainer::advance(u_int32_t num_frames)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_samples.empty() || m_samples[0].empty()) {
        return;
    }

    const u_int64_t channel_size = m_samples[0].size();

    if (m_looping && channel_size > 0) {
        m_read_position = (m_read_position + num_frames) % channel_size;
    } else {
        m_read_position += num_frames;
        if (m_read_position > channel_size) {
            m_read_position = channel_size;
        }
    }
}

bool SoundFileContainer::is_read_at_end() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_samples.empty() || m_samples[0].empty()) {
        return true;
    }

    return m_read_position >= m_samples[0].size();
}

void SoundFileContainer::reset_read_position()
{
    set_read_position(0);
}

std::vector<double> SoundFileContainer::get_normalized_preview(u_int32_t channel, u_int32_t max_points) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (channel >= m_samples.size() || m_samples[channel].empty() || max_points == 0) {
        return std::vector<double>();
    }

    const auto& channel_data = m_samples[channel];
    const u_int64_t channel_size = channel_data.size();

    std::vector<double> preview;
    preview.reserve(max_points);

    if (channel_size <= max_points) {
        return channel_data;
    }

    auto [min_it, max_it] = std::minmax_element(channel_data.begin(), channel_data.end());
    double min_val = *min_it;
    double max_val = *max_it;
    double range = max_val - min_val;

    if (std::abs(range) < 1e-6) {
        preview.resize(max_points, 0.0);
        return preview;
    }

    double step = static_cast<double>(channel_size) / max_points;

    for (u_int32_t i = 0; i < max_points; ++i) {
        u_int64_t start_idx = static_cast<u_int64_t>(i * step);
        u_int64_t end_idx = static_cast<u_int64_t>((i + 1) * step);
        if (end_idx > channel_size)
            end_idx = channel_size;
        if (start_idx >= end_idx)
            start_idx = end_idx - 1;

        double seg_min = channel_data[start_idx];
        double seg_max = channel_data[start_idx];

        for (u_int64_t j = start_idx + 1; j < end_idx; ++j) {
            if (channel_data[j] < seg_min)
                seg_min = channel_data[j];
            if (channel_data[j] > seg_max)
                seg_max = channel_data[j];
        }

        double value = (std::abs(seg_min) > std::abs(seg_max)) ? seg_min : seg_max;

        preview.push_back((value - min_val) / range * 2.0 - 1.0);
    }

    return preview;
}

std::vector<std::pair<std::string, u_int64_t>> SoundFileContainer::get_markers() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_markers;
}

u_int64_t SoundFileContainer::get_marker_position(const std::string& marker_name) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (const auto& marker : m_markers) {
        if (marker.first == marker_name) {
            return marker.second;
        }
    }

    return 0;
}

void SoundFileContainer::add_region_group(const RegionGroup& group)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_region_groups[group.name] = group;
}

void SoundFileContainer::add_region_point(const std::string& group_name, const RegionPoint& point)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_region_groups[group_name].points.push_back(point);
}

const RegionGroup& SoundFileContainer::get_region_group(const std::string& group_name) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_region_groups.at(group_name);
}

const std::unordered_map<std::string, RegionGroup> SoundFileContainer::get_all_region_groups() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_region_groups;
}

const std::vector<double>& SoundFileContainer::get_raw_samples(uint32_t channel) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    static const std::vector<double> empty_vector;

    if (is_interleaved() && !m_samples.empty()) {
        return m_samples[0];
    }

    if (channel < m_samples.size()) {
        return m_samples[channel];
    }

    return empty_vector;
}

std::vector<std::vector<double>>& SoundFileContainer::get_all_raw_samples()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_samples;
}

const std::vector<std::vector<double>>& SoundFileContainer::get_all_raw_samples() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_samples;
}

void SoundFileContainer::set_raw_samples(const std::vector<double>& samples, uint32_t channel)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (channel == 0 && m_num_channels > 1) {
        m_samples.clear();
        m_samples.resize(1);
        m_samples[0] = samples;

        m_num_frames = samples.size() / m_num_channels;

    } else {
        if (channel >= m_samples.size()) {
            m_samples.resize(channel + 1);
        }
        m_samples[channel] = samples;

        if (channel >= m_num_channels) {
            m_num_channels = channel + 1;
        }

        m_num_frames = samples.size();
    }

    if (m_default_processor) {
        m_ready_for_processing = true;
        if (m_processing_state == ProcessingState::IDLE) {
            update_processing_state(ProcessingState::READY);
        }
    }

    if (!m_buffers.empty() && m_buffers[channel]) {
        m_buffers[channel]->clear();
    }
}

void SoundFileContainer::set_all_raw_samples(const std::vector<std::vector<double>>& samples)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (samples.empty()) {
        return;
    }

    m_samples = samples;
    m_num_channels = static_cast<u_int32_t>(samples.size());

    u_int64_t num_frames = 0;
    if (!samples[0].empty()) {
        num_frames = samples[0].size();
    }

    m_buffers.clear();

    if (m_processed_data.size() != samples.size()) {
        m_processed_data.resize(samples.size());
        for (auto& channel : m_processed_data) {
            channel.resize(512, 0.0);
        }
    }

    if (m_default_processor) {
        m_ready_for_processing = true;
        if (m_processing_state == ProcessingState::IDLE) {
            update_processing_state(ProcessingState::READY);
        }
    }
}

void SoundFileContainer::set_looping(bool enable)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_looping = enable;
}

bool SoundFileContainer::get_looping() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_looping;
}

void SoundFileContainer::set_default_processor(std::shared_ptr<DataProcessor> processor)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_default_processor) {
        m_default_processor->on_detach(shared_from_this());
    }

    m_default_processor = processor;

    if (m_default_processor && !m_samples.empty()) {
        m_default_processor->on_attach(shared_from_this());
        m_ready_for_processing = true;
    }
}

std::shared_ptr<DataProcessor> SoundFileContainer::get_default_processor() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_default_processor;
}

std::shared_ptr<DataProcessingChain> SoundFileContainer::get_processing_chain()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_processing_chain;
}

void SoundFileContainer::set_processing_chain(std::shared_ptr<DataProcessingChain> chain)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_processing_chain = chain;
}

void SoundFileContainer::mark_buffers_for_processing(bool should_process)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (auto& buffer : m_buffers) {
        buffer->mark_for_processing(should_process);
        buffer->enforce_default_processing(should_process);
    }
}

void SoundFileContainer::mark_buffers_for_removal()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (auto& buffer : m_buffers) {
        buffer->mark_for_removal();
    }
}

std::shared_ptr<Buffers::AudioBuffer> SoundFileContainer::get_channel_buffer(u_int32_t channel)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_buffers.empty()) {
        create_container_buffers();
    }

    if (channel >= m_buffers.size()) {
        return nullptr;
    }

    return m_buffers[channel];
}

std::vector<std::shared_ptr<Buffers::AudioBuffer>> SoundFileContainer::get_all_buffers()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_buffers.empty()) {
        create_container_buffers();
    }
    return m_buffers;
}

u_int32_t SoundFileContainer::get_sample_rate() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_sample_rate;
}

u_int32_t SoundFileContainer::get_num_audio_channels() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_num_channels;
}

u_int64_t SoundFileContainer::get_num_frames_total() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_num_frames;
}

double SoundFileContainer::get_duration_seconds() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_sample_rate <= 0) {
        return 0.0;
    }

    return static_cast<double>(m_num_frames) / m_sample_rate;
}

void SoundFileContainer::lock()
{
    m_mutex.lock();
}

void SoundFileContainer::unlock()
{
    m_mutex.unlock();
}

bool SoundFileContainer::try_lock()
{
    return m_mutex.try_lock();
}

ProcessingState SoundFileContainer::get_processing_state() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_processing_state;
}

void SoundFileContainer::update_processing_state(ProcessingState new_state)
{
    if (m_processing_state == new_state) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (new_state == ProcessingState::READY) {
        m_channels_consumed_this_cycle.clear();
    }

    if (new_state == ProcessingState::NEEDS_REMOVAL) {
        mark_buffers_for_removal();
    }

    m_processing_state = new_state;

    if (m_state_callback) {
        m_state_callback(shared_from_this(), new_state);
    }
}

void SoundFileContainer::register_state_change_callback(std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_state_callback = callback;
}

void SoundFileContainer::unregister_state_change_callback()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_state_callback = nullptr;
}

void SoundFileContainer::register_channel_reader(u_int32_t channel)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_active_channel_readers[channel]++;
}

void SoundFileContainer::unregister_channel_reader(u_int32_t channel)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_active_channel_readers[channel]) {
        if (--m_active_channel_readers[channel] <= 0) {
            m_active_channel_readers.erase(channel);
        }
    }
}

bool SoundFileContainer::has_active_channel_readers() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return !m_active_channel_readers.empty();
}

void SoundFileContainer::mark_channel_consumed(u_int32_t channel)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_channels_consumed_this_cycle.insert(channel);
}

bool SoundFileContainer::all_channels_consumed() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& [channel, count] : m_active_channel_readers) {
        if (m_channels_consumed_this_cycle.find(channel) == m_channels_consumed_this_cycle.end()) {
            return false;
        }
    }
    return true;
}

std::vector<std::vector<double>>& SoundFileContainer::get_processed_data()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_processed_data;
}

const std::vector<std::vector<double>>& SoundFileContainer::get_processed_data() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_processed_data;
}

void SoundFileContainer::process_default()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_default_processor) {
        update_processing_state(ProcessingState::PROCESSING);

        m_default_processor->process(shared_from_this());

        update_processing_state(ProcessingState::PROCESSED);
    }
}
}
