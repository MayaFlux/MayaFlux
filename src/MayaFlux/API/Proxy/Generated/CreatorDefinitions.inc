std::shared_ptr<MayaFlux::Nodes::Generator::Sine> Creator::Sine(float frequency, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Sine>(std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Sine", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Sine> Creator::Sine(const std::shared_ptr<MayaFlux::Nodes::Node> & frequency_modulator, float frequency, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Sine>(std::forward<decltype(frequency_modulator)>(frequency_modulator), std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Sine", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Sine> Creator::Sine(float frequency, const std::shared_ptr<MayaFlux::Nodes::Node> & amplitude_modulator, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Sine>(std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude_modulator)>(amplitude_modulator), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Sine", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Sine> Creator::Sine(const std::shared_ptr<MayaFlux::Nodes::Node> & frequency_modulator, const std::shared_ptr<MayaFlux::Nodes::Node> & amplitude_modulator, float frequency, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Sine>(std::forward<decltype(frequency_modulator)>(frequency_modulator), std::forward<decltype(amplitude_modulator)>(amplitude_modulator), std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Sine", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Phasor> Creator::Phasor(float frequency, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Phasor>(std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Phasor", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Phasor> Creator::Phasor(const std::shared_ptr<MayaFlux::Nodes::Node> & frequency_modulator, float frequency, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Phasor>(std::forward<decltype(frequency_modulator)>(frequency_modulator), std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Phasor", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Phasor> Creator::Phasor(float frequency, const std::shared_ptr<MayaFlux::Nodes::Node> & amplitude_modulator, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Phasor>(std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude_modulator)>(amplitude_modulator), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Phasor", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Phasor> Creator::Phasor(const std::shared_ptr<MayaFlux::Nodes::Node> & frequency_modulator, const std::shared_ptr<MayaFlux::Nodes::Node> & amplitude_modulator, float frequency, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Phasor>(std::forward<decltype(frequency_modulator)>(frequency_modulator), std::forward<decltype(amplitude_modulator)>(amplitude_modulator), std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Phasor", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Impulse> Creator::Impulse(float frequency, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Impulse>(std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Impulse", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Impulse> Creator::Impulse(const std::shared_ptr<MayaFlux::Nodes::Node> & frequency_modulator, float frequency, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Impulse>(std::forward<decltype(frequency_modulator)>(frequency_modulator), std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Impulse", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Impulse> Creator::Impulse(float frequency, const std::shared_ptr<MayaFlux::Nodes::Node> & amplitude_modulator, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Impulse>(std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude_modulator)>(amplitude_modulator), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Impulse", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Impulse> Creator::Impulse(const std::shared_ptr<MayaFlux::Nodes::Node> & frequency_modulator, const std::shared_ptr<MayaFlux::Nodes::Node> & amplitude_modulator, float frequency, double amplitude, float offset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Impulse>(std::forward<decltype(frequency_modulator)>(frequency_modulator), std::forward<decltype(amplitude_modulator)>(amplitude_modulator), std::forward<decltype(frequency)>(frequency), std::forward<decltype(amplitude)>(amplitude), std::forward<decltype(offset)>(offset));
    MF_LIVE_EXPOSE_NAMED("Impulse", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Logic> Creator::Logic(double threshold)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Logic>(std::forward<decltype(threshold)>(threshold));
    MF_LIVE_EXPOSE_NAMED("Logic", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Logic> Creator::Logic(MayaFlux::Nodes::Generator::LogicOperator op, double threshold)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Logic>(std::forward<decltype(op)>(op), std::forward<decltype(threshold)>(threshold));
    MF_LIVE_EXPOSE_NAMED("Logic", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Logic> Creator::Logic(MayaFlux::Nodes::Generator::Logic::DirectFunction function)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Logic>(std::forward<decltype(function)>(function));
    MF_LIVE_EXPOSE_NAMED("Logic", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Logic> Creator::Logic(MayaFlux::Nodes::Generator::Logic::MultiInputFunction function, size_t input_count)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Logic>(std::forward<decltype(function)>(function), std::forward<decltype(input_count)>(input_count));
    MF_LIVE_EXPOSE_NAMED("Logic", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Logic> Creator::Logic(MayaFlux::Nodes::Generator::Logic::SequentialFunction function, size_t history_size)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Logic>(std::forward<decltype(function)>(function), std::forward<decltype(history_size)>(history_size));
    MF_LIVE_EXPOSE_NAMED("Logic", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Logic> Creator::Logic(MayaFlux::Nodes::Generator::Logic::TemporalFunction function)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Logic>(std::forward<decltype(function)>(function));
    MF_LIVE_EXPOSE_NAMED("Logic", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Polynomial> Creator::Polynomial(const std::vector<double> & coefficients)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Polynomial>(std::forward<decltype(coefficients)>(coefficients));
    MF_LIVE_EXPOSE_NAMED("Polynomial", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Polynomial> Creator::Polynomial(MayaFlux::Nodes::Generator::Polynomial::DirectFunction function)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Polynomial>(std::forward<decltype(function)>(function));
    MF_LIVE_EXPOSE_NAMED("Polynomial", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Polynomial> Creator::Polynomial(MayaFlux::Nodes::Generator::Polynomial::BufferFunction function, MayaFlux::Nodes::Generator::PolynomialMode mode, size_t buffer_size)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Polynomial>(std::forward<decltype(function)>(function), std::forward<decltype(mode)>(mode), std::forward<decltype(buffer_size)>(buffer_size));
    MF_LIVE_EXPOSE_NAMED("Polynomial", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Random> Creator::Random(Kinesis::Stochastic::Algorithm type)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Random>(std::forward<decltype(type)>(type));
    MF_LIVE_EXPOSE_NAMED("Random", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Counter> Creator::Counter(uint32_t modulo, int32_t step)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Counter>(std::forward<decltype(modulo)>(modulo), std::forward<decltype(step)>(step));
    MF_LIVE_EXPOSE_NAMED("Counter", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Generator::Counter> Creator::Counter(const std::shared_ptr<MayaFlux::Nodes::Node> & reset_trigger, uint32_t modulo, int32_t step)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Generator::Counter>(std::forward<decltype(reset_trigger)>(reset_trigger), std::forward<decltype(modulo)>(modulo), std::forward<decltype(step)>(step));
    MF_LIVE_EXPOSE_NAMED("Counter", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Filters::IIR> Creator::IIR(const std::shared_ptr<MayaFlux::Nodes::Node> & input, const std::vector<double> & a_coef, const std::vector<double> & b_coef)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Filters::IIR>(std::forward<decltype(input)>(input), std::forward<decltype(a_coef)>(a_coef), std::forward<decltype(b_coef)>(b_coef));
    MF_LIVE_EXPOSE_NAMED("IIR", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Filters::IIR> Creator::IIR(const std::vector<double> & a_coef, const std::vector<double> & b_coef)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Filters::IIR>(std::forward<decltype(a_coef)>(a_coef), std::forward<decltype(b_coef)>(b_coef));
    MF_LIVE_EXPOSE_NAMED("IIR", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Filters::FIR> Creator::FIR(const std::shared_ptr<MayaFlux::Nodes::Node> & input, const std::vector<double> & coeffs)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Filters::FIR>(std::forward<decltype(input)>(input), std::forward<decltype(coeffs)>(coeffs));
    MF_LIVE_EXPOSE_NAMED("FIR", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Filters::FIR> Creator::FIR(const std::vector<double> & coeffs)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Filters::FIR>(std::forward<decltype(coeffs)>(coeffs));
    MF_LIVE_EXPOSE_NAMED("FIR", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::GlyphGeometryNode> Creator::GlyphGeometryNode(std::string text, float pen_x, float pen_y, float tolerance)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::GlyphGeometryNode>(std::forward<decltype(text)>(text), std::forward<decltype(pen_x)>(pen_x), std::forward<decltype(pen_y)>(pen_y), std::forward<decltype(tolerance)>(tolerance));
    MF_LIVE_EXPOSE_NAMED("GlyphGeometryNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::GlyphGeometryNode> Creator::GlyphGeometryNode(Portal::Text::FontFace & face, uint32_t pixel_size, std::string text, float pen_x, float pen_y, float tolerance)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::GlyphGeometryNode>(std::forward<decltype(face)>(face), std::forward<decltype(pixel_size)>(pixel_size), std::forward<decltype(text)>(text), std::forward<decltype(pen_x)>(pen_x), std::forward<decltype(pen_y)>(pen_y), std::forward<decltype(tolerance)>(tolerance));
    MF_LIVE_EXPOSE_NAMED("GlyphGeometryNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::MeshWriterNode> Creator::MeshWriterNode(size_t initial_vertex_capacity)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::MeshWriterNode>(std::forward<decltype(initial_vertex_capacity)>(initial_vertex_capacity));
    MF_LIVE_EXPOSE_NAMED("MeshWriterNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::SDFNode> Creator::SDFNode(Kinesis::SpatialField field, const glm::vec3 & bounds_min, const glm::vec3 & bounds_max, uint32_t res_x, uint32_t res_y, uint32_t res_z, float iso_level)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::SDFNode>(std::forward<decltype(field)>(field), std::forward<decltype(bounds_min)>(bounds_min), std::forward<decltype(bounds_max)>(bounds_max), std::forward<decltype(res_x)>(res_x), std::forward<decltype(res_y)>(res_y), std::forward<decltype(res_z)>(res_z), std::forward<decltype(iso_level)>(iso_level));
    MF_LIVE_EXPOSE_NAMED("SDFNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::PathGeneratorNode> Creator::PathGeneratorNode(Kinesis::InterpolationMode mode, Eigen::Index samples_per_segment, size_t max_control_points, double tension)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::PathGeneratorNode>(std::forward<decltype(mode)>(mode), std::forward<decltype(samples_per_segment)>(samples_per_segment), std::forward<decltype(max_control_points)>(max_control_points), std::forward<decltype(tension)>(tension));
    MF_LIVE_EXPOSE_NAMED("PathGeneratorNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::PathGeneratorNode> Creator::PathGeneratorNode(MayaFlux::Nodes::GpuSync::PathGeneratorNode::CustomPathFunction custom_func, Eigen::Index samples_per_segment, size_t max_control_points)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::PathGeneratorNode>(std::forward<decltype(custom_func)>(custom_func), std::forward<decltype(samples_per_segment)>(samples_per_segment), std::forward<decltype(max_control_points)>(max_control_points));
    MF_LIVE_EXPOSE_NAMED("PathGeneratorNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::PointCollectionNode> Creator::PointCollectionNode(size_t initial_capacity)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::PointCollectionNode>(std::forward<decltype(initial_capacity)>(initial_capacity));
    MF_LIVE_EXPOSE_NAMED("PointCollectionNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::PointCollectionNode> Creator::PointCollectionNode(std::vector<MayaFlux::Nodes::PointVertex> points)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::PointCollectionNode>(std::forward<decltype(points)>(points));
    MF_LIVE_EXPOSE_NAMED("PointCollectionNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::TopologyGeneratorNode> Creator::TopologyGeneratorNode(Kinesis::ProximityMode mode, bool auto_connect, size_t max_points)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::TopologyGeneratorNode>(std::forward<decltype(mode)>(mode), std::forward<decltype(auto_connect)>(auto_connect), std::forward<decltype(max_points)>(max_points));
    MF_LIVE_EXPOSE_NAMED("TopologyGeneratorNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::TopologyGeneratorNode> Creator::TopologyGeneratorNode(MayaFlux::Nodes::GpuSync::TopologyGeneratorNode::CustomConnectionFunction custom_func, bool auto_connect, size_t max_points)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::TopologyGeneratorNode>(std::forward<decltype(custom_func)>(custom_func), std::forward<decltype(auto_connect)>(auto_connect), std::forward<decltype(max_points)>(max_points));
    MF_LIVE_EXPOSE_NAMED("TopologyGeneratorNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::LineSegmentsNode> Creator::LineSegmentsNode(size_t initial_capacity)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::LineSegmentsNode>(std::forward<decltype(initial_capacity)>(initial_capacity));
    MF_LIVE_EXPOSE_NAMED("LineSegmentsNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::ProceduralTextureNode> Creator::ProceduralTextureNode(uint32_t width, uint32_t height)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::ProceduralTextureNode>(std::forward<decltype(width)>(width), std::forward<decltype(height)>(height));
    MF_LIVE_EXPOSE_NAMED("ProceduralTextureNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::GpuSync::ProceduralTextureNode> Creator::ProceduralTextureNode(uint32_t width, uint32_t height, MayaFlux::Nodes::GpuSync::ProceduralTextureNode::PixelGenerator generator)
{
    auto obj = std::make_shared<MayaFlux::Nodes::GpuSync::ProceduralTextureNode>(std::forward<decltype(width)>(width), std::forward<decltype(height)>(height), std::forward<decltype(generator)>(generator));
    MF_LIVE_EXPOSE_NAMED("ProceduralTextureNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::StreamReaderNode> Creator::StreamReaderNode()
{
    auto obj = std::make_shared<MayaFlux::Nodes::StreamReaderNode>();
    MF_LIVE_EXPOSE_NAMED("StreamReaderNode", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Constant> Creator::Constant(double value)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Constant>(std::forward<decltype(value)>(value));
    MF_LIVE_EXPOSE_NAMED("Constant", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::ModalNetwork> Creator::ModalNetwork(size_t num_modes, double fundamental, MayaFlux::Nodes::Network::ModalNetwork::Spectrum spectrum, double base_decay)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::ModalNetwork>(std::forward<decltype(num_modes)>(num_modes), std::forward<decltype(fundamental)>(fundamental), std::forward<decltype(spectrum)>(spectrum), std::forward<decltype(base_decay)>(base_decay));
    MF_LIVE_EXPOSE_NAMED("ModalNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::ModalNetwork> Creator::ModalNetwork(const std::vector<double> & frequency_ratios, double fundamental, double base_decay)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::ModalNetwork>(std::forward<decltype(frequency_ratios)>(frequency_ratios), std::forward<decltype(fundamental)>(fundamental), std::forward<decltype(base_decay)>(base_decay));
    MF_LIVE_EXPOSE_NAMED("ModalNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::WaveguideNetwork> Creator::WaveguideNetwork(MayaFlux::Nodes::Network::WaveguideNetwork::WaveguideType type, double fundamental_freq, double sample_rate)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::WaveguideNetwork>(std::forward<decltype(type)>(type), std::forward<decltype(fundamental_freq)>(fundamental_freq), std::forward<decltype(sample_rate)>(sample_rate));
    MF_LIVE_EXPOSE_NAMED("WaveguideNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::ResonatorNetwork> Creator::ResonatorNetwork(size_t num_resonators, MayaFlux::Nodes::Network::ResonatorNetwork::FormantPreset preset)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::ResonatorNetwork>(std::forward<decltype(num_resonators)>(num_resonators), std::forward<decltype(preset)>(preset));
    MF_LIVE_EXPOSE_NAMED("ResonatorNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::ResonatorNetwork> Creator::ResonatorNetwork(const std::vector<double> & frequencies, const std::vector<double> & q_values)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::ResonatorNetwork>(std::forward<decltype(frequencies)>(frequencies), std::forward<decltype(q_values)>(q_values));
    MF_LIVE_EXPOSE_NAMED("ResonatorNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::RelationNetwork> Creator::RelationNetwork()
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::RelationNetwork>();
    MF_LIVE_EXPOSE_NAMED("RelationNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::ParticleNetwork> Creator::ParticleNetwork(size_t num_particles, const glm::vec3 & bounds_min, const glm::vec3 & bounds_max, Kinesis::SpatialDistribution init_mode)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::ParticleNetwork>(std::forward<decltype(num_particles)>(num_particles), std::forward<decltype(bounds_min)>(bounds_min), std::forward<decltype(bounds_max)>(bounds_max), std::forward<decltype(init_mode)>(init_mode));
    MF_LIVE_EXPOSE_NAMED("ParticleNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::PointCloudNetwork> Creator::PointCloudNetwork()
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::PointCloudNetwork>();
    MF_LIVE_EXPOSE_NAMED("PointCloudNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::PointCloudNetwork> Creator::PointCloudNetwork(size_t num_points, const glm::vec3 & bounds_min, const glm::vec3 & bounds_max, Kinesis::SpatialDistribution init_mode)
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::PointCloudNetwork>(std::forward<decltype(num_points)>(num_points), std::forward<decltype(bounds_min)>(bounds_min), std::forward<decltype(bounds_max)>(bounds_max), std::forward<decltype(init_mode)>(init_mode));
    MF_LIVE_EXPOSE_NAMED("PointCloudNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::InstanceNetwork> Creator::InstanceNetwork()
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::InstanceNetwork>();
    MF_LIVE_EXPOSE_NAMED("InstanceNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::MeshNetwork> Creator::MeshNetwork()
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::MeshNetwork>();
    MF_LIVE_EXPOSE_NAMED("MeshNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Nodes::Network::AssemblyNetwork> Creator::AssemblyNetwork()
{
    auto obj = std::make_shared<MayaFlux::Nodes::Network::AssemblyNetwork>();
    MF_LIVE_EXPOSE_NAMED("AssemblyNetwork", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::AudioBuffer> Creator::AudioBuffer()
{
    auto obj = std::make_shared<MayaFlux::Buffers::AudioBuffer>();
    MF_LIVE_EXPOSE_NAMED("AudioBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::AudioBuffer> Creator::AudioBuffer(uint32_t channel_id, uint32_t num_samples)
{
    auto obj = std::make_shared<MayaFlux::Buffers::AudioBuffer>(std::forward<decltype(channel_id)>(channel_id), std::forward<decltype(num_samples)>(num_samples));
    MF_LIVE_EXPOSE_NAMED("AudioBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::NodeBuffer> Creator::NodeBuffer(uint32_t channel_id, uint32_t num_samples, std::shared_ptr<Nodes::Node> source, bool clear_before_process)
{
    auto obj = std::make_shared<MayaFlux::Buffers::NodeBuffer>(std::forward<decltype(channel_id)>(channel_id), std::forward<decltype(num_samples)>(num_samples), std::forward<decltype(source)>(source), std::forward<decltype(clear_before_process)>(clear_before_process));
    MF_LIVE_EXPOSE_NAMED("NodeBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::FeedbackBuffer> Creator::FeedbackBuffer(uint32_t channel_id, uint32_t num_samples, float feedback, uint32_t feed_samples)
{
    auto obj = std::make_shared<MayaFlux::Buffers::FeedbackBuffer>(std::forward<decltype(channel_id)>(channel_id), std::forward<decltype(num_samples)>(num_samples), std::forward<decltype(feedback)>(feedback), std::forward<decltype(feed_samples)>(feed_samples));
    MF_LIVE_EXPOSE_NAMED("FeedbackBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::SoundContainerBuffer> Creator::SoundContainerBuffer(uint32_t channel_id, uint32_t num_samples, const std::shared_ptr<Kakshya::StreamContainer> & container, uint32_t source_channel)
{
    auto obj = std::make_shared<MayaFlux::Buffers::SoundContainerBuffer>(std::forward<decltype(channel_id)>(channel_id), std::forward<decltype(num_samples)>(num_samples), std::forward<decltype(container)>(container), std::forward<decltype(source_channel)>(source_channel));
    MF_LIVE_EXPOSE_NAMED("SoundContainerBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::VKBuffer> Creator::VKBuffer(size_t size_bytes, MayaFlux::Buffers::VKBuffer::Usage usage, Kakshya::DataModality modality)
{
    auto obj = std::make_shared<MayaFlux::Buffers::VKBuffer>(std::forward<decltype(size_bytes)>(size_bytes), std::forward<decltype(usage)>(usage), std::forward<decltype(modality)>(modality));
    MF_LIVE_EXPOSE_NAMED("VKBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::VKBuffer> Creator::VKBuffer()
{
    auto obj = std::make_shared<MayaFlux::Buffers::VKBuffer>();
    MF_LIVE_EXPOSE_NAMED("VKBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::NodeTextureBuffer> Creator::NodeTextureBuffer(std::shared_ptr<Nodes::GpuSync::TextureNode> node, std::string binding_name)
{
    auto obj = std::make_shared<MayaFlux::Buffers::NodeTextureBuffer>(std::forward<decltype(node)>(node), std::forward<decltype(binding_name)>(binding_name));
    MF_LIVE_EXPOSE_NAMED("NodeTextureBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::TextureBuffer> Creator::TextureBuffer(uint32_t width, uint32_t height, Portal::Graphics::ImageFormat format, const void * initial_pixel_data)
{
    auto obj = std::make_shared<MayaFlux::Buffers::TextureBuffer>(std::forward<decltype(width)>(width), std::forward<decltype(height)>(height), std::forward<decltype(format)>(format), std::forward<decltype(initial_pixel_data)>(initial_pixel_data));
    MF_LIVE_EXPOSE_NAMED("TextureBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::GeometryBuffer> Creator::GeometryBuffer(std::shared_ptr<Nodes::GpuSync::GeometryWriterNode> node, const std::string & binding_name, float over_allocate_factor)
{
    auto obj = std::make_shared<MayaFlux::Buffers::GeometryBuffer>(std::forward<decltype(node)>(node), std::forward<decltype(binding_name)>(binding_name), std::forward<decltype(over_allocate_factor)>(over_allocate_factor));
    MF_LIVE_EXPOSE_NAMED("GeometryBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::CompositeGeometryBuffer> Creator::CompositeGeometryBuffer(size_t initial_capacity, float over_allocate_factor)
{
    auto obj = std::make_shared<MayaFlux::Buffers::CompositeGeometryBuffer>(std::forward<decltype(initial_capacity)>(initial_capacity), std::forward<decltype(over_allocate_factor)>(over_allocate_factor));
    MF_LIVE_EXPOSE_NAMED("CompositeGeometryBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::MeshBuffer> Creator::MeshBuffer(Kakshya::MeshData data)
{
    auto obj = std::make_shared<MayaFlux::Buffers::MeshBuffer>(std::forward<decltype(data)>(data));
    MF_LIVE_EXPOSE_NAMED("MeshBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::MeshBuffer> Creator::MeshBuffer(std::shared_ptr<Nodes::GpuSync::MeshWriterNode> node)
{
    auto obj = std::make_shared<MayaFlux::Buffers::MeshBuffer>(std::forward<decltype(node)>(node));
    MF_LIVE_EXPOSE_NAMED("MeshBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::MeshNetworkBuffer> Creator::MeshNetworkBuffer(std::shared_ptr<Nodes::Network::MeshNetwork> network, float over_allocate_factor)
{
    auto obj = std::make_shared<MayaFlux::Buffers::MeshNetworkBuffer>(std::forward<decltype(network)>(network), std::forward<decltype(over_allocate_factor)>(over_allocate_factor));
    MF_LIVE_EXPOSE_NAMED("MeshNetworkBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::ComputeMeshBuffer> Creator::ComputeMeshBuffer(MayaFlux::Buffers::ComputeMeshBuffer::Config config)
{
    auto obj = std::make_shared<MayaFlux::Buffers::ComputeMeshBuffer>(std::forward<decltype(config)>(config));
    MF_LIVE_EXPOSE_NAMED("ComputeMeshBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::ComputeMeshBuffer> Creator::ComputeMeshBuffer(Kinesis::SpatialField field, const glm::vec3 & bounds_min, const glm::vec3 & bounds_max, uint32_t res_x, uint32_t res_y, uint32_t res_z, float iso_level)
{
    auto obj = std::make_shared<MayaFlux::Buffers::ComputeMeshBuffer>(std::forward<decltype(field)>(field), std::forward<decltype(bounds_min)>(bounds_min), std::forward<decltype(bounds_max)>(bounds_max), std::forward<decltype(res_x)>(res_x), std::forward<decltype(res_y)>(res_y), std::forward<decltype(res_z)>(res_z), std::forward<decltype(iso_level)>(iso_level));
    MF_LIVE_EXPOSE_NAMED("ComputeMeshBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::ComputeMeshBuffer> Creator::ComputeMeshBuffer(const glm::vec3 & bounds_min, const glm::vec3 & bounds_max, uint32_t res_x, uint32_t res_y, uint32_t res_z, float iso_level, std::string field_shader)
{
    auto obj = std::make_shared<MayaFlux::Buffers::ComputeMeshBuffer>(std::forward<decltype(bounds_min)>(bounds_min), std::forward<decltype(bounds_max)>(bounds_max), std::forward<decltype(res_x)>(res_x), std::forward<decltype(res_y)>(res_y), std::forward<decltype(res_z)>(res_z), std::forward<decltype(iso_level)>(iso_level), std::forward<decltype(field_shader)>(field_shader));
    MF_LIVE_EXPOSE_NAMED("ComputeMeshBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::NetworkGeometryBuffer> Creator::NetworkGeometryBuffer(std::shared_ptr<Nodes::Network::NodeNetwork> network, const std::string & binding_name, float over_allocate_factor)
{
    auto obj = std::make_shared<MayaFlux::Buffers::NetworkGeometryBuffer>(std::forward<decltype(network)>(network), std::forward<decltype(binding_name)>(binding_name), std::forward<decltype(over_allocate_factor)>(over_allocate_factor));
    MF_LIVE_EXPOSE_NAMED("NetworkGeometryBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::NetworkTextureBuffer> Creator::NetworkTextureBuffer(std::shared_ptr<Nodes::Network::NodeNetwork> network, std::shared_ptr<Core::VKImage> texture, const std::string & binding_name, float over_allocate_factor)
{
    auto obj = std::make_shared<MayaFlux::Buffers::NetworkTextureBuffer>(std::forward<decltype(network)>(network), std::forward<decltype(texture)>(texture), std::forward<decltype(binding_name)>(binding_name), std::forward<decltype(over_allocate_factor)>(over_allocate_factor));
    MF_LIVE_EXPOSE_NAMED("NetworkTextureBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::InstanceNetworkBuffer> Creator::InstanceNetworkBuffer(std::shared_ptr<Nodes::Network::InstanceNetwork> network, float over_allocate_factor)
{
    auto obj = std::make_shared<MayaFlux::Buffers::InstanceNetworkBuffer>(std::forward<decltype(network)>(network), std::forward<decltype(over_allocate_factor)>(over_allocate_factor));
    MF_LIVE_EXPOSE_NAMED("InstanceNetworkBuffer", obj);
    return obj;
}

std::shared_ptr<MayaFlux::Buffers::NetworkAudioBuffer> Creator::NetworkAudioBuffer(uint32_t channel_id, uint32_t num_samples, std::shared_ptr<Nodes::Network::NodeNetwork> network, bool clear_before_process)
{
    auto obj = std::make_shared<MayaFlux::Buffers::NetworkAudioBuffer>(std::forward<decltype(channel_id)>(channel_id), std::forward<decltype(num_samples)>(num_samples), std::forward<decltype(network)>(network), std::forward<decltype(clear_before_process)>(clear_before_process));
    MF_LIVE_EXPOSE_NAMED("NetworkAudioBuffer", obj);
    return obj;
}
