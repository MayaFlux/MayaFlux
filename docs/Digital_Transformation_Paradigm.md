# Digital Transformation Paradigms: Thinking in Data Flow

- [Introduction](#introduction)
- [MayaFlux](#mayaflux)
  - [Nodes](#nodes)
    - [Definitions](#definitions)
    - [Flow logic](#flow-logic)
    - [Processing as events](#processing-as-events)
  - [Buffers](#buffers)
    - [Definitions](#definitions-1)
    - [Processing](#processing)
    - [Processing chain](#processing-chain)
    - [Custom processing](#custom-processing)
  - [Coroutines: Time as a Creative Material](#coroutines-time-as-a-creative-material)
    - [The Temporal Paradigm](#the-temporal-paradigm)
    - [Definitions](#definitions-2)
    - [Temporal Domains and Coordination](#temporal-domains-and-coordination)
    - [Kriya Temporal Patterns](#kriya-temporal-patterns)
    - [EventChains and Temporal Composition](#eventchains-and-temporal-composition)
    - [Buffer Integration and Capture](#buffer-integration-and-capture)
  - [Containers](#containers)
    - [Data Architecture](#data-architecture)
    - [Data Modalities and Detection](#data-modalities-and-detection)
    - [SoundFileContainer: Foundational Implementation](#soundfilecontainer-foundational-implementation)
    - [Region-Based Data Access](#region-based-data-access)
    - [RegionGroups and Metadata](#regiongroups-and-metadata)
- [Digital Data Flow Architecture](#digital-data-flow-architecture)%

# Introduction

MayaFlux represents a fundamental rethinking of creative
computation—moving beyond analog-inspired metaphors and file-based
workflows into true digital-first paradigms. Rather than simulating
hardware or accommodating legacy constraints, MayaFlux treats data
transformation as the primary creative medium.

This shift requires new ways of thinking about creative processes.
Instead of "programming" versus "composing," MayaFlux recognizes that
data transformation is creative expression. Mathematical relationships
become creative decisions, temporal coordination becomes compositional
structure, and multi-dimensional data access becomes creative material selection.

Critically, MayaFlux moves beyond the artificial disciplinary separation of
audio and visual processing that dominates contemporary creative software. Audio
samples at 48 kHz, pixels at 60 Hz, and compute results all flow through
identical transformation primitives—because they are all just data. The only
meaningful difference is the timing context (domain) in which they're processed.
A node that transforms one unit at a time operates identically whether that
unit is a float sample, a pixel, or a compute result. A buffer that accumulates
moments until release works identically for audio, visual, or spectral data. The
paradigms that follow are domain-agnostic by design.

The architecture emerges from four foundational paradigms
that work compositionally rather than separately:

- Nodes: Unit-by-unit transformation moments where information becomes
  something new
- Buffers: Temporal gathering spaces where individual moments accumulate
  into collective expressions
- Coroutines: Time as malleable creative material enabling complex
  temporal coordination
- Containers: Multi-dimensional data structures that treat information
  as compositional material

Each paradigm operates with computational precision while remaining expressively
flexible. They compose naturally across audio, visual, and compute domains,
creating complex multi-modal creative workflows from simple, well-defined
transformation primitives.

This document explores each paradigm through practical examples that demonstrate
both their individual capabilities and their compositional relationships across
computational domains. The focus remains on digital thinking—embracing
computational possibilities that have no analog equivalent, and treating all data
(whether it sounds, displays, or computes) through unified transformation
infrastructure.

# MayaFlux

## Nodes

Nodes represent _moments of transformation_ in the digital
reality—points where information becomes something new. Each node
processes _one unit at a time_, creating precise, intentional change in
the flow of data.

Think of nodes not as devices or tools, but as _creative decisions made
manifest_. They can craft new information (generators) or reshape
existing streams (processors). Any concept that can be expressed as
unit-by-unit transformation can become a Node.

Nodes embody mathematical relationships through polynomials, make
logical decisions through boolean operations, channel chaos through
stochastic patterns, sculpt existing flows through filtering, or
manifest familiar synthesis paradigms like sine waves, phasors, and
impulses.

The rate of processing i.e how often individual units evaluate and transition to the
next—is governed by the concept of `Domains`. A node can operate in the `Audio`
domain (processing at sample rate), the `Graphics` domain (processing at frame
rate), the `Compute` domain (processing at GPU compute rate), or `Custom` domains
(user-defined rates). The fundamental transformation logic remains identical;
only the timing context.

### Definitions

Nodes can be defined using the fluent mechanism, API level convenience
wrappers or via directly creating a modern c++ shared pointers.

```cpp
// Fluent
auto wave = vega.Sine(440.0f).channel(0) | Audio;
auto noise = vega.Random(GUASSIAN).domain(Audio)[1];
auto texture_distort = vega.Polynomial({0.1, 0.8, 2.0}) | Graphics;
auto pulse = vega.Impulse(2.0) | Audio;

// API Wrappers
auto envelope = MayaFlux::ceate_node<Shape>(0.0f, 1.0f, 2.5f);
MayaFlux::register_audio_node(envelope, {0, 1});

auto random_walk = MayaFlux::create_node<Stochastic>(PERLIN, 0.1f);
MayaFlux::register_audio_node(random_walk, 1);

auto logic_gate = MayaFlux::create_node<Logic>(([](double input) { return input > 0.5; }));

// Direct method
auto custom_filter = std::make_shared<Nodes::Processor::IIR>(
    std::vector<double>{0.2, 0.4, 0.2},  // feedforward coefficients
    std::vector<double>{1.0, -0.5}       // feedback coefficients
);
custom_filter->set_input(random_walk);
get_node_graph_manager()->add_node_to_root(ProcessingToken::AUDIO_RATE, 0);
```

Each path serves different creative moments—fluid thinking, structured creation, or precise manipulation.
precise manipulation. The key insight: the node definition syntax is identical
across audio, visual, and compute domains; only the semantic content (what the
node does) changes.

### Flow logic

Nodes connect through `>>` creating `streams of transformation`. Each
connection point represents a decision about how information should
evolve. Critically, these streams work across domains:

```cpp
phasor >> noise >> (IIR(sine) * 0.5) + noise >> DAC;

// Cross-domain connection: audio drives visual parameters
auto audio_envelope = vega.Sine(1.0f) | Audio;
auto visual_scale = vega.Polynomial({0.1, 1.0, 0.5});

// Audio tick fires at sample rate, modulating visual node
audio_envelope >> (visual_scale * 2.0) >> screen_geometry;
```

The flow doesn't just pass data—it creates _relationships_ where each
transformation influences and is influenced by its neighbors.

### Processing as events

Since nodes operate with unit-by-unit precision, they become
`temporal anchors` places where you can attach other creative processes
to the exact rhythm of transformation:

```cpp
// Sync external processes to the pulse of transformation
auto clock = vega.Impulse(4.0) | Audio;
clock->on_tick([](NodeContext& ctx) {
    // Trigger visual events at 4Hz rhythm
    schedule_visual_pulse(ctx.value, ctx.timestamp);
});

// React to mathematical conditions becoming true
auto wave = vega.Sine(0.5f) | Audio;
wave->on_tick_if([](NodeContext& ctx) {
    return ctx.value > 0.8;
}, [](NodeContext ctx) {
    // Musical events triggered by wave peaks
    trigger_chain_event_at(ctx.timestamp);
});

// Logic states create temporal regions for other processes
auto gate = vega.Logic([](double input) { return input > 0.0; });
gate->while_true([](NodeContext& ctx) {
    // Continuous processes that exist only during "true" time
    modulate_global_clock(ctx.value * 0.7);
});

// Mathematical relationships become event generators
auto envelope = vega.Polynomial({1.0, -0.5, 0.1});
envelope->on_change([](NodeContext& ctx) {
    // React to any change in polynomial output
    update_filter_cutoff(ctx.value * 2000.0 + 200.0);
});
```

This turns `computational precision into creative opportunity` every
processing moment becomes a potential point of creative intervention.

## Buffers

Buffers are temporal gatherers – they don't store data, they accumulate
moments until they reach fullness, then release their contents and await
the next collection.

Think of them as breathing spaces in the data flow, where individual
units gather into collective expressions. Unlike nodes that transform
unit-by-unit, buffers work with accumulated time – they collect until
they have enough information to pass along as a temporal block.

A buffer's life cycle is simple: gather → release → await → gather. This
cycle creates the temporal chunking that makes certain kinds of
transformation possible - operations that need to see patterns across
time rather than individual moments. This lifecycle is identical whether
the buffer collects audio samples, pixels, or compute results.

### Definitions

The _transient collectors_ paradigm of buffers allow them to work with
more than just `float` or `long float` (double) data types. They can
work with images, textures, text (string) basically any simple data type
accommodated by default in C++. However, we will focus only on
AudioBuffers in this document.

The default audio buffer simply creates a buffer that works with high
precision decimals or `doubles`. MayaFlux also provides other buffer
types derived from `AudioBuffers` And much like nodes, buffers have
multiple ways of creating them.

```cpp
// Fluent
auto audio_buf = vega.AudioBuffer()[1] | Audio;
auto vk_buf = vega.VKBUffer(64, ::Usage::VERTEX,
                              ::DataModality::VERTEX_COLORS_RGBA) | Graphics;

// Convenience API Sound
auto wave = MayaFlux::create_node<Sine>();
auto node_buffer = MayaFlux::create_buffer<NodeBuffer>(0, 512, sine);
MayaFlux::register_audio_buffer(node_buffer, 0);

// Convenience API Graphics
auto logic = MayaFlux::create_node<Logic>();
auto update = MayaFlux::create_processor<TextureBindingsProcessor>(vk_buf);
update->bind_texture_node("toggle_noise_reflection", update, vk_buf);

// Explicit
auto stream_sources = std::make_shared<MayaFlux::Kakshya::DynamicStreamSource>(48000, 2);
auto stream_buf = std::make_shared<MayaFlux::Buffers::ContainerBuffer>(0, 512, stream_sources);
buffer_manager->add_buffer(stream_buf, ProcessingToken::AUDIO_PARALLEL, 0);

auto staging = std::make_shared<MayaFlux::Buffers::VKBuffer>(0, Usage::STAGING, DataModality::UNKNOWN);
buffer_manager->add_buffer(staging, ProcessingToken::GRAPHICS_BACKEND);
```

### Processing

Unlike nodes that are essentially mathematical expressions evaluated
unit by unit, buffers are simply temporal accumulators. However, there
is little expressive or computational value in simply _holding on_ to
data for a short period, do nothing, and pass it along.

Buffers work with `BufferProcessors` that are attached to them to handle
operations on the accumulated data.

It might help to visualize buffers as facilitators where processing
_happens_ to them, in place of them handling any transformations
themselves.

Every buffer carries a default processor—its natural way of handling
accumulated data. While this is an external tool applied to the buffer;
it is the buffer's inherent behavior for each cycle The `NodeBuffer`
from earlier example uses `NodeProcessor` that evaluates the specified
node unit by unit, until buffer size is complete. The stream buffer has
a default processor `AccessProcessor` that "acquires" concurrent 512
samples from the stream source container The basic audio buffer has no
default processor but it exposes methods to attach one.

```cpp
auto buffer = vega.AudioBuffer();
buffer->set_default_processor(FeedbackProcessor);
```

The default processor defines the buffer's natural expression—what it
does with accumulated data when left to its own behavior, or how it
accumulates data to begin with.

### Processing chain

`BufferProcessors` are standalone features that require a buffer to
process, but they are not specific to a single buffer. Each processor
can be attached to multiple buffers, be it the same instance of the
processor, or several instances of the same class to different buffers.
More information on buffers and processors can be found _here_

On the other hand, buffers themselves accommodate multiple processors
using `BufferProcessingChain`

```cpp
auto buffer = vega.AudioBuffer();

auto feedback = MayaFlux::create_processor<FeedbackProcessor>(buffer);
auto node = vega.Polynomial([](auto x) {return x *= (sqrt(x));});

auto poly_processor = MayaFlux::create_processor<PolynomialProcessor>(buffer, node);

auto chain = MayaFlux::create_processing_chain();

chain->add_processor(poly_processor);
chain->add_processor(feedback);

buffer->set_processing_chain(chain);

// Explicit
auto upload = std::make_shared<MayaFlux::Buffers::BufferUploadProcessor>();
upload->configure_source(staging, vk_buf);
auto g_chain = std::make_shared<MayaFlux::Buffers::BufferProcessingChain>();
g_chain->add_processor(upload, vk_buf);
vk_buf->set_processing_chain(g_chain);
```

It is important to note that the order of execution of processing chains
are sequential, determined by the order of registration.

### Custom processing

Apart from the available list of default processors, it is very trivial
to create your own processors.

```cpp
class my_processor : public BufferProcessor {
public:
    // Processing function is what gets evaluated each buffer cycle
    void processing_function(std::shared_ptr<Buffer> buffer) override
    {
        auto& data = buffer->get_data();
        // do something here to data;
    }

    // you can also override init and end behaviours
    void on_attach(std::shared_ptr<Buffer> buffer) override {}
    void on_detach(std::shared_ptr<Buffer> buffer) override {}
};
```

If you are unfamiliar with creating C++ classes or your overall exposure
to programming does not yet accommodate such explicit creation paradigm,
MayaFlux offers several convenience methods to achieve the same.

1.  Quick processors

    A quick process can by any function that accepts a buffer as input,
    and applies any desired operation on the buffer's data. These quick
    processes allow _using_ any previously available value/data inside
    the function. The specifics of how they work will require an
    understanding of lamdas in programming, but suffice to say that
    \[…\] these braces allow you to mention any data you want to use;

    ```cpp

    auto noise = vega.Random(UNIFORM);
    auto my_num = 3.16f;

    MayaFlux::attach_quick_process([noise, my_num](auto buf){
                    // Actual processing
                    auto& data = buffer->get_data();
                    for (auto& sample : data)
                    {
                        sample *= noise->process_sample(my_num);
                    }};
    )
    ```

    Quick processes can be attached to individual buffers, to
    `RootAudioBuffer` i.e all buffers in a single channel, or to all
    channels via pre/post process globals.

    ```cpp
    MayaFlux::attach_quick_process(func, buffer);

    MayaFlux::attach_quick_process_to_channel(func, channel_id);

    MayaFlux::register_process_hook(func, PREPROCESS);
    ```

## Coroutines: Time as a Creative Material

Coroutines represent a fundamental shift in how we think about time in
digital creation. They're not "sequencers" that trigger events, but
temporal coordination primitives that let different processes develop
their own relationship with time while remaining connected to the larger
creative flow. Think of coroutines as living temporal expressions – each
one can pause, wait, coordinate with others, and resume according to its
own creative logic. They transform time from a linear constraint into a
malleable creative material that you can shape, stretch, layer, and
weave together. In MayaFlux, coroutines exist in two interconnected
namespaces: `Vruta` (the scheduling infrastructure) and `Kriya` (the
creative temporal patterns). Together, they create a system where time
becomes compositional.

### The Temporal Paradigm

Unlike traditional sequencing, coroutines let each process maintain its
own sense of time while coordinating with others:

```cpp
// Each coroutine develops its own temporal relationship
auto slow_drift = create_temporal_process([](auto scheduler) -> Vruta::SoundRoutine {
    while (true) {
        co_await SampleDelay{48000};  // Wait 1 second at 48kHz
        adjust_global_latency(0.001f); // Subtle drift
    }
});

auto rhythmic_pulse = create_temporal_process([](auto scheduler) -> Vruta::SoundRoutine {
    while (true) {
        co_await SampleDelay{12000};  // Wait 0.25 seconds
        trigger_percussive_event();
    }
});
```

Each coroutine co<sub>awaits</sub> different temporal conditions,
creating multimodal time where multiple temporal flows coexist and
interact.

### Definitions

Coroutines exist in two interconnected namespaces: `Vruta` (scheduling
infrastructure) and `Kriya` (creative temporal patterns). Together they
transform time from constraint into compositional material.

Much like Buffers and Nodes, coroutines have fluent flow methods, global
convenience API and explicit definition support. The primary difference
is `vega` does not have any coroutine representation as `vega` enables
simple non-verbose creation and not time manipulate **at** creation.

```cpp
// Fluent API
auto shape_node = vega.Polynomial({0.1, 0.5, 2.f});
shape_node >> Time(2.f);
auto wave = vega.Phasor(shape_node, 440.f);
NodeTimer::play_for(wave, 5.f);

// Convenience wrappers
MayaFlux::schedule_metro(2.0, []() {
    modulate_filter_cutoff();
}, "main_clock");

MayaFlux::schedule_pattern([](uint64_t beat) {
    return x % 8 == 0;  // Every 8th beat
}, []() {
    change_distribution();
}, 1.0, "Update noise");

// Create coordinated temporal events
auto event_chain = MayaFlux::create_event_chain()
    .then([]() { start_clock(); }, 0.0)
    .then([]() { trigger_buffer_copy(); }, 0.1)
    .then([]() { start_input_capture(); }, 0.5)
    .then([]() { reset_state(); }, 2.0);


// Explicit API
auto complex_pattern = [](Vruta::TaskScheduler& scheduler) -> Vruta::SoundRoutine {
    auto& promise = co_await Kriya::GetAudioPromise{};

    while (true) {
        // Wait for condition
        co_await Kriya::Gate{scheduler, []() {
            schedule_another_clock();
        }, logic_node, true};

        // Dynamic timing based on musical state
        float wait_time = calculate_musical_timing();
        co_await SampleDelay{scheduler.seconds_to_samples(wait_time)};

        // Coordinate with other temporal processes
        co_await Kriya::Trigger{scheduler, true, []() {
            sync_frame_clock();
        }, sync_node};
    }
};
```

### Temporal Domains and Coordination

Coroutines coordinate across computational domains through specialized
awaiters that understand each context's timing characteristics:

```cpp
// Domain-specific temporal anchors
co_await SampleDelay{1024};     // Audio domain precision
co_await FrameDelay{2};         // Visual domain coordination
co_await MultiRateDelay{samples: 512, frames: 1};  // Cross-domain sync

// Temporal coordination with state memory
auto evolving_process = [](auto scheduler) -> Vruta::SoundRoutine {
    auto& promise = co_await Kriya::GetAudioPromise{};
    promise.set_state("phase", 0.0f);

    while (!promise.should_terminate) {
        float* phase = promise.get_state<float>("phase");
        *phase += 0.01f;

        trigger_event_at(*phase);
        co_await SampleDelay{static_cast<uint64_t>(*phase * 1000)};
    }
};
```

### Kriya Temporal Patterns

Kriya provides creative temporal primitives that capture common temporal
relationships:

```cpp
// Regular temporal pulse
auto metro = Kriya::metro(*scheduler, 0.5, pulse_callback);

// Temporal event choreography
auto sequence = Kriya::sequence(*scheduler, {
    {0.0, initialize_state},
    {0.25, transform_data},
    {0.75, complete_cycle}
});

// Continuous parameter interpolation
auto line = Kriya::line(*scheduler, 0.0f, 1.0f, 2.0f, 44, false);

// Access interpolated values by name
schedule_task("envelope", std::move(line));
float* current_value = get_line_value("envelope");
```

### EventChains and Temporal Composition

EventChains create temporal choreography through fluent composition:

```cpp
// Temporal event sequence
auto chain = Kriya::EventChain{}
    .then([]() { start_process(); }, 0.0)
    .then([]() { modulate_filter(); }, 0.125)
    .then([]() { trigger_release(); }, 0.5);
chain.start();

// Node and time sequencing through operators
auto sequence = Kriya::Sequence{};
sequence >> Play(sine_node) >> Wait(0.5) >> Action(reset_state) >> Wait(0.25);
sequence.execute();

// Temporal operators for direct node control
sine_node >> Time(2.0);  // Play node for 2 seconds
filter_node >> DAC::instance();  // Route to output
```

### Buffer Integration and Capture

Kriya coordinates with buffer processing for temporal data capture and
analysis:

```cpp
// Buffer capture coordination
auto capture = Kriya::BufferCapture{audio_buffer}
    .for_cycle(4)
    .with_window(1024, 0.5f)  // Windowed analysis
    .as_circular(2048);       // Circular buffer mode

// Temporal buffer coordination
auto coordinator = Kriya::CycleCoordinator{*scheduler};

// Synchronize multiple buffer pipelines
auto sync_routine = coordinator.sync_pipelines({
    std::ref(spectral_pipeline),
    std::ref(temporal_pipeline)
}, 4);  // Sync every 4 cycles

// Manage transient data lifecycle
auto data_routine = coordinator.manage_transient_data(
    buffer,
    [](uint32_t cycle) { process_data(cycle); },
    [](uint32_t cycle) { cleanup_data(cycle); }
);
```

This creates _temporal architecture_ where time becomes malleable
creative material, enabling coordination across nodes, buffers, and
computational domains through sample-accurate timing and compositional
temporal relationships.

## Containers

Containers represent a paradigm shift in creative data management—moving
beyond traditional file-based workflows into dynamic, multi-dimensional
data structures that treat information as compositional material. Unlike
conventional audio tools that separate "files" from "processing,"
containers unify data storage, transformation, and access into coherent
creative units.

This requires forethought into creative data management because existing
digital tools rarely provide infrastructure for general creative data
categorization. Containers in MayaFlux recognize that creative data
exists in multiple modalities (audio, visual, spectral), dimensions
(time, space, frequency), and organizational structures (regions,
groups, attributes) that should be accessible through unified interfaces
rather than disparate format-specific tools.

Containers work with \***\*DataVariant\*\*** (unified type storage), \***\*DataDimensions\*\*** (structural descriptors), \***\*Regions\*\***
(spatial/temporal selections), and \***\*RegionGroups\*\*** (organized
collections) to create a coherent data architecture that scales from
simple audio files to complex multi-modal creative datasets.

### Data Architecture

Containers organize data through dimensional thinking rather than format
constraints:

```cpp
// DataVariant: Unified type storage for different precision needs
DataVariant audio_data = std::vector<double>{...};        // High precision
DataVariant image_data = std::vector<uint8_t>{...};       // 8-bit image
DataVariant spectral_data = std::vector<std::complex<float>>{...}; // Complex FFT

// DataDimensions: Structural descriptors
auto time_dim = DataDimension::time(48000, "samples");
auto channel_dim = DataDimension::channel(2, 1);
auto spatial_dim = DataDimension::spatial(1024, 'x', 1);

// Regions: N-dimensional selections
Region intro_section = Region::audio_span(0, 12000, 0, 1);  // First 0.25 seconds, both channels
Region frequency_band = Region::spectral_band(440, 880);     // Specific frequency range

// RegionGroups: Organized collections with metadata
RegionGroup song_structure;
song_structure.add_region("spectra", spectral_peak_region, {{"noisyness", "moderate"}, {"tempo", 120}});
song_structure.add_region("end", end_region, {{"zero_crossings", "high"}, {"tempo", 125}});
```

### Data Modalities and Detection

Containers automatically detect and adapt to different data modalities
based on dimensional structure:

```cpp
// Automatic modality detection from dimensions
std::vector<DataDimension> audio_dims = {
    DataDimension::time(48000),
    DataDimension::channel(2)
};
DataModality modality = detect_data_modality(audio_dims);  // Returns AUDIO_MULTICHANNEL

// Supported modalities
enum class DataModality {
    AUDIO_1D,           // 1D audio signal
    AUDIO_MULTICHANNEL, // Multi-channel audio
    IMAGE_2D,           // 2D grayscale image
    IMAGE_COLOR,        // 2D color image
    VIDEO_GRAYSCALE,    // 3D video (time + 2D)
    SPECTRAL_2D,        // 2D spectral data
    TENSOR_ND,          // N-dimensional tensor
    VOLUMETRIC_3D       // 3D volumetric data
};
```

### SoundFileContainer: Foundational Implementation

SoundFileContainer demonstrates the container paradigm through practical
audio file handling:

```cpp
// Fluent creation
auto sound_file = vega.read_audio("sample.wav").channels({0, 1});

// Convenience API
auto container = MayaFlux::load_sound_file("sample.wav");
auto duration = container->get_duration_seconds();

// Explicit construction
auto file_container = std::make_shared<SoundFileContainer>(48000, 2);
file_container->setup(96000, 48000, 2);  // 2 seconds, 48kHz, stereo

// Region-based access
Region loop_region = Region::audio_span(24000, 72000, 0, 1);  // 0.5s to 1.5s
DataVariant loop_data = container->get_region_data(loop_region);

// Memory layout control
container->set_memory_layout(MemoryLayout::COLUMN_MAJOR);  // Optimize for channel processing
```

### Region-Based Data Access

Regions provide precise, multi-dimensional data selection without
copying entire datasets:

```cpp
// Temporal regions
Region intro = Region::audio_span(0, 12000, 0, 1);        // First 0.25 seconds
Region outro = Region::audio_span(84000, 96000, 0, 1);    // Last 0.25 seconds

// Channel-specific regions
Region left_channel = Region::audio_span(0, 96000, 0, 0); // Left channel only
Region stereo_mix = Region::audio_span(12000, 84000, 0, 1); // Middle section, both channels

// Extract and process regions
DataVariant intro_data = container->get_region_data(intro);
auto intro_samples = std::get<std::vector<double>>(intro_data);

// Modify regions in place
DataVariant silence = create_silence(6000);  // 0.125 seconds of silence
container->set_region_data(intro, silence);  // Replace intro with silence
```

### RegionGroups and Metadata

RegionGroups organize multiple regions with rich metadata for creative
workflow management:

```cpp
// Create organized region collection
RegionGroup compositional_structure("Composition Structure");

// Add regions with creative metadata
compositional_structure.add_region("intro", intro_region, {
    {"energy_level", "low"},
    {"harmonic_content", "sparse"},
    {"tempo", 85}
});

compositional_structure.add_region("variation 1", var1_region, {
    {"energy_level", "moderate"},
    {"noise_density", "high"},
    {"tempo", 120}
});

// Query regions by creative attributes
auto high_energy_regions = find_regions_with_attribute(compositional_structure, "energy_level", "high");
auto moderate_tempo_regions = find_regions_with_attribute(compositional_structure, "tempo", 120);

// Attach region groups to containers
container->add_region_group(song_structure);
```

This creates _creative data architecture_ where information becomes
compositional material that can be selected, transformed, and organized
through precise multi-dimensional access patterns, enabling workflows
that treat data as a creative medium rather than static files.

# Digital Data Flow Architecture

These four paradigms—Nodes, Buffers, Coroutines, and Containers—form the
foundational data transformation architecture of MayaFlux. Together,
they create a unified computational environment where information flows
through precise transformations rather than being constrained by analog
metaphors or format boundaries.

\***\*Nodes\*\*** provide unit-by-unit transformation precision, turning
mathematical relationships into creative decisions. \***\*Buffers\*\***
create temporal gathering spaces where individual moments accumulate
into collective expressions. \***\*Coroutines\*\*** transform time itself
into compositional material, enabling complex temporal coordination
across multiple processing domains. \***\*Containers\*\*** organize
multi-dimensional data as creative material, supporting everything from
simple audio files to complex cross-modal datasets.

The power emerges from their compositional relationships:

```cpp
// Data flows through unified transformation architecture
auto spectral_node = vega.Polynomial({0.1, 0.8, 2.0});
auto temporal_buffer = vega.AudioBuffer()[0] | Audio;
auto coordination_routine = Kriya::metro(*scheduler, 0.25, [&]() {
    auto region = container->get_region_data(analysis_region);
    temporal_buffer->apply_processor(spectral_node);
});

// Cross-domain coordination through shared timing
spectral_node >> temporal_buffer >> container_analysis;
coordination_routine.start();
```

This architecture enables _digital-first creative thinking_ where
computational precision becomes creative opportunity. Unit-by-unit
transformations coordinate with temporal accumulation, while coroutines
orchestrate complex timing relationships across multi-dimensional data
structures.

Rather than separating "programming" from "composing," MayaFlux treats
data transformation as the fundamental creative act. Mathematical
relationships, temporal coordination, and dimensional data access become
unified aspects of a single creative expression.

The next stage of this digital paradigm involves [Domains and Control](Domain_and_Control.md)
— how these transformation primitives coordinate across different
computational contexts and how creative control emerges from the
interaction between precise computational timing and expressive creative
intent.
