# Domains and Control: Computational Contexts in Digital Creation

- [Domains and Control: Computational Contexts in Digital Creation](#domains-and-control-computational-contexts-in-digital-creation)
- [Processing Tokens: Computational Identity](#processing-tokens-computational-identity)
  - [Nodes::ProcessingTokens](#nodesprocessingtokens)
  - [Buffers::ProcessingTokens](#buffersprocessingtokens)
  - [Vruta::ProcessingTokens](#vrutaprocessingtokens)
  - [Domain Composition: Unified Computational Environments](#domain-composition-unified-computational-environments)
- [Engine Control vs User Control: Computational Autonomy](#engine-control-vs-user-control-computational-autonomy)
  - [Nodes](#nodes)
    - [NodeGraphManager](#nodegraphmanager)
      - [Explicit user control](#explicit-user-control)
    - [RootNode](#rootnode)
    - [Direct Node Management](#direct-node-management)
      - [Chaining](#chaining)
  - [Buffers](#buffers)
    - [BufferManager](#buffermanager)
    - [RootBuffer](#rootbuffer)
    - [Direct buffer management and processing](#direct-buffer-management-and-processing)
  - [Coroutines](#coroutines)
    - [TaskScheduler](#taskscheduler)
    - [Kriya](#kriya)
    - [Clock Systems](#clock-systems)
    - [Direct Coroutine Management](#direct-coroutine-management)
      - [Self-managed SoundRoutine Creation](#self-managed-soundroutine-creation)
      - [API-based Awaiter Patterns](#api-based-awaiter-patterns)
    - [Direct Routine Control and State Management](#direct-routine-control-and-state-management)
    - [Multi-domain Coroutine Coordination](#multi-domain-coroutine-coordination)%

Digital creative systems require more than individual transformation units—they need _computational contexts_ that coordinate timing, resource allocation, and execution strategies across different processing requirements.
MayaFlux introduces `Domains` as unified computational environments where Nodes, Buffers, and Coroutines operate with shared understanding of temporal precision, execution location, and coordination patterns.

Rather than forcing all processes into a single temporal framework, Domains enable _multi-modal computational thinking_ where audio-rate precision, visual-frame coordination, and custom temporal patterns coexist and interact naturally. Each Domain represents a complete processing configuration that spans all three subsystems, creating coherent computational environments for different creative requirements.

# Processing Tokens: Computational Identity

Each subsystem defines its processing characteristics through `ProcessingTokens`: computational identities that specify how information should be handled within that domain:

## Nodes::ProcessingTokens

Considering the unit by unit processing nature of Nodes, the domains pertain to the rate at which each unit is processed. Hence, Nodes support the following `ProcessingTokens`

- **AUDIO_RATE:** processes information at sample-level precision, making unit-by-unit transformations synchronized with audio timing.

- **VISUAL_RATE:** operates at frame-level coordination, enabling visual transformations that align with display refresh patterns.

- **CUSTOM_RATE:** provides user-defined temporal precision for computational patterns that transcend traditional audio or visual constraints.

## Buffers::ProcessingTokens

As the processors attached to buffers operate on the entire data collection, the domain system for Buffers require different methodologies and accommodate new features.
It is simply not limited to rate of processing but also the device onto which the processing frame can be offloaded to. Batch processing also affords features such as sequential vs parallel processing.
`Buffers::ProcessingToken` contain following set of bitfield composition to specify execution characteristics:

- **SAMPLE_RATE:** evaluates buffer-sized chunks at audio rate, and **FRAME_RATE:** processes frame-based data blocks.
  They are mutually exclusive

- **CPU_PROCESS** and **GPU_PROCESS** define execution location, and are mutually exclusive

- **SEQUENTIAL** and **PARALLEL** control concurrency patterns, and are mutually exclusive.

There are following combined tokens:

- **AUDIO_BACKEND** = `SAMPLE_RATE` + `CPU_PROCESS` + `SEQUENTIAL`
- **GRAPHICS_BACKEND** = `FRAME_RATE` + `GPU_PROCESS` + `PARALLEL`
- **AUDIO_PARALLEL** = `SAMPLE_RATE` + `GPU_PROCESS` + `PARALLEL`

## Vruta::ProcessingTokens

Coroutines need similar processing tokens as Nodes, i.e tick rate accuracy. Coroutines also benefit from being available to `suspend`, `resume` or `restart` on demand.

Routines configured via `Vruta` (and `Scheduler`) can be configured to use the following tokens:

- **SAMPLE_ACCURATE** provides sample-level temporal precision for audio-synchronized coroutines.

- **FRAME_ACCURATE** aligns with visual frame timing.

- **MULTI_RATE** adapts dynamically between different temporal patterns.

- **ON_DEMAND** enables event-driven execution.

- **CUSTOM** supports user-defined coordination patterns.

## Domain Composition: Unified Computational Environments

Domains combine these tokens into coherent computational contexts using bitfield composition. Each Domain represents a complete processing configuration:

```cpp
// Audio processing with sample-accurate coordination
Domain::AUDIO = Nodes::ProcessingToken::AUDIO_RATE + Buffers::ProcessingToken::AUDIO_BACKEND + Vruta::ProcessingToken::SAMPLE_ACCURATE

// Graphics with frame-accurate synchronization
Domain::GRAPHICS = Nodes::ProcessingToken::VISUAL_RATE + Buffers::ProcessingToken::GRAPHICS_BACKEND + Vruta::ProcessingToken::FRAME_ACCURATE

// Parallel audio processing with GPU acceleration
Domain::AUDIO_GPU = Nodes::ProcessingToken::AUDIO_RATE + Buffers::ProcessingToken::GPU_PROCESS + Vruta::ProcessingToken::MULTI_RATE
```

This composition enables **domain decomposition** where complex computational requirements can be broken into constituent processing characteristics and recombined as needed:

```cpp
auto node_token = get_node_token(Domain::AUDIO_GPU);
auto buffer_token = get_buffer_token(Domain::AUDIO_GPU);
auto task_token = get_task_token(Domain::AUDIO_GPU);

// Create custom domain from individual tokens
auto custom_domain = compose_domain(
    Nodes::CUSTOM_RATE,
    Buffers::FRAME_RATE | Buffers::CPU_PROCESS | Buffers::PARALLEL,
    Vruta::ON_DEMAND
);
```

Domains enable **cross-modal coordination** where different temporal patterns interact naturally:

```cpp
// Audio-visual synchronization
auto sync_domain = Domain::AUDIO_VISUAL_SYNC;
auto spectral_node = vega.fft() | sync_domain;
auto visual_buffer = graphics_buffer | sync_domain;

// Data flows between temporal contexts while maintaining coordination
spectral_node >> visual_buffer;
```

# Engine Control vs User Control: Computational Autonomy

MayaFlux operates on a **default automation with expressive override** philosophy. The engine provides intelligent automation for common creative workflows while enabling precise user control when specific computational patterns are required.
This is also tightly coupled with the philosophy that every practical aspect of the API should yield itself to overrides, substitution or disabling.

By default, operations use the engine's managed systems for optimal performance and coordination: This pertains to `Nodes`, `Buffers` and `Coroutines`. Containers, due to their non cyclical nature do not have enforced engine defaults, but can.

Engine management happens through multiple systems and managers for each paradigm, and at every step they require explicit domain specification (often automated to defaults via API wrappers).

Here is a breakdown of each component flow in engine management and examples for overriding with user control.

## Nodes

The `Engine` class that functions as the default coordinator and lifecyle manager for _Backends_ and _Subsystems_ also manages the central node coordinator called `NodeGraphManager`.

While the aforementioned backends, subsystems and Engine itself can be untangled from central management and replaced with different systems, that is a conversation for a different time.

### NodeGraphManager

```cpp
// Fluent API - Engine handles domain assignment and registration
auto sine_node = vega.Sine(440.0f) | Audio;     // Automatic AUDIO_RATE domain
auto noise_gen = vega.Random(GAUSSIAN)[1];  // Automatic channel 1 assignment

// API Wrappers - Engine manages registration and token assignment
auto envelope = MayaFlux::create_node<Shape>(0.0f, 1.0f, 2.5f);  // Auto-registered to default channel
auto filter = MayaFlux::create_node<IIR>(lowpass_coeffs);        // Auto-assigned AUDIO_RATE token

// Engine handles node graph coordination
sine_node >> filter >> envelope;  // Automatic connection and flow management
```

The engine's NodeGraphManager automatically:

- Assigns processing tokens based on domain specifications
- Registers nodes with appropriate channel routing
- Manages root node hierarchies across different processing tokens
- Coordinates timing and execution across multiple domains

When initializing using vega, `vega.Sine()[0] | Audio`, the instruction is to create node -> get default NodeGraphManger from engine -> register it for Channel 0's root -> at `Domain::Audio`, which resolves to `Nodes::ProcessingToken::AUDIO_RATE`

#### Explicit user control

The same node can be registered directly with `NodeGraphManager::add_to_root(shared_ptr(node), ProcessingToken, channel)`

Calling `MayaFlux::create_node` is functionally identical to `vega`, except the _Domain_ is implicitly initialized to `Audio` by default.

Every aspect of Node management can be controlled explicitly for precise computational patterns:

```cpp
// Direct token processing registration
node_manager->register_token_processor(
    Nodes::ProcessingToken::CUSTOM_RATE,
    [](std::span<RootNode*> roots) {
        // Custom processing logic for all CUSTOM_RATE nodes
        for (auto* root : roots) {
            root->process_custom_algorithm();
        }
    }
);

// Manual channel mask management
node_manager->set_channel_mask(custom_node, 0b0110);  // Channels 1 and 2
node_manager->unset_channel_mask(custom_node, 0b0010); // Remove channel 1
```

The control is not just limited `NodeGraphManager` internals. It is possible to replace the Engine's default node graph manager.
`get_context()->get_node_graph_manger() = std::make_shared<Nodes::NodeGraphManager>(args)`

### RootNode

When a node is registered to a channel in NodeGraphManger, it is being added to a `RootNode`. There is only one root node per processing token per channel as it acts as the central registry and lock-free processing stage manager for nodes.

RootNode exposes `process_sample()` and `process_batch(num_samples)` which can be called externally. The process callback checks channel registration-processing state of each node, handles processing of each node, requests node state reset for the channel RootNode is operating on and sums all samples.

`RootNode` itself does not operate based on `ProcessingTokens`, but one is required at construction to facilitate Engine integration. When `NodeGraphManager` is initialized by the Engine, it automatically sets up RootNodes based on Token and number of channels.

Root Nodes can be used outside of the channel context (or outside of NodeGraphManger context -> Engine Context), as RootNode still provides the most optimal and lock-free way of coordinating process() of multiple nodes.

Use `RootNode::register_node(shared_ptr node)` to add a node to Root. The registration triggers a guarded atomic operation that checks for current processing state, and adds the node only when it is safe. `RootNode::unregister_node` behaves the same for removing a node.

_Note:_ As `RootNode` only handles its own graph, it is unaware of registration across channels beyond processing state check. So, adding or removing from root does not update the channel registration status (bitmask) of the node.

### Direct Node Management

Nodes need not be added to `RootNode` or `NodeGraphManager` to enable processing. Calling `node->process_sample()` or `node->process_batch(num_samples)` evaluates the same as any automated procedure.

```cpp
auto pulse = vega.Impulse(200.f);
// Process a node once every 2 seconds
MayaFlux::schedule_metro(2, [pulse](){
    pulse->process_sample();
});
```

#### Chaining

The examples shown previously `node1 >> node2` or `node1 * node2` are fluent methods for chaining, and Engine registration occurs implicitly.

The first example is facilitated using a type of node called `ChainNode`.
When using `>>` overload, `ChainNode::initialize()` is called which registers the nodes with Engine methods.

The second example creates a type of node called `BinaryOpNode` that handles a binary operation on the node's output as registered by a callback handle.
And like `ChainNode`, the fluent `* or +` calls `BinaryOpNode::initialize()` for engine registration

```cpp
auto pulse = vega.Impulse(20.f);
auto wave = vega.Sine(880.f);
// No engine registration
auto chain_node = std::make_shared<Nodes::ChainNode>(pulse, wave);
// Processes both nodes using sequence combination logic
chain_node->process_sample();

// Self initialized binary operation node with custom operation logic
// No engine registration
auto bin_op_node = std::make_shared<Nodes::BinaryOpNode>(pulse, wave, [&](double v1, double v2){
    v1 *= v2;
    return sqrt(v1 + v2);
});

// processes both nodes and applies operation
bin_op_node->process_sample()
```

## Buffers

Similar to `NodeGraphManager`, _Engine_ also handles lifecyle and visibility management of buffers via `BufferManager`. The role of the buffer manger is to:

- register buffers to specific tokens and channels
- handle input buffers from hardware backend
- handle processing sync, chaining and concurrency
- Handle data exchange between channels
- Handle normalization and final processes

### BufferManager

```cpp
auto temporal_buffer = vega.AudioBuffer()[0] | Parallel;        // Automatic AUDIO_PARALLEL configuration
auto feedback_buffer = vega.FeedbackBuffer(0.7f).domain(Audio).channel(0);   // Auto-assigned processing characteristics

auto wave = vega.Sine();
auto Node_buffer = MayaFlux::create_buffer<Buffers::NodeBuffer>(0, 512, wave); // Implicitly added to engine audio domain
auto proc = MayaFlux::create_processor<Buffers::StreamWriteProcessor>(temporal_buffer); // Automatic process registration
get_buffer_manger()->supply_buffer_to(temporal_buffer, 1); // Automatically send buffer output to channel 1 of audio domain

```

When using fluent structure `vega.AudioBuffer[0] | Parallel`, the instruction is to create `AudioBuffer` -> set it to channel 0, get default buffer manager from engine -> register to `AUDIO_PARALLEL` token.

Using `MayaFlux::create_buffer` or `::create`\_any_buffer_namespace_method, it internally evaluates to creating the specified entity and handling default registration procedure with `Engine` controlled `BufferManager`.

Direct creation methods for the above:

```cpp
auto manager = MayaFlux::get_buffer_manager();
manager->add_audio_buffer(buffer, ProcessingToken::CUSTOM, 0);
manager->create_buffer<NodeBuffer>(ProcessingToken::AUDIO_BACKEND, 1, 512, wave);

auto proc = std::make_shared<StreamProcessor>(temporal_buffer);
manager->add_processor(proc, temporal_buffer);
manager->supply_buffer_to(temporal_buffer, ProcessingToken::GPU_BACKEND, 1);
```

Much like `NodeGraphManager`, it is possible to create custom processing functions

```cpp
manager->register_token_processor(ProcessingToken::CUSTOM, (std::vector<std::shared_ptr<RootAudioBuffer>> buffers){
    for (auto& buffer: buffers) {
        buffer->process_default();
        for (auto& child : buffer->get_child_buffers()) {
            if (auto processing_chain = child->get_processing_chain()) {
                if (child->has_data_for_cycle()) {
                    processing_chain->process(child);
                }
            }
        }
    }
});
```

### RootBuffer

Similar to `RootNode` in nodes, when adding a buffer to a channel in `BufferManager`, it is added to that channel's `RootBuffer`. As this document focuses on audio, `RootAudioBuffer` will be used as the exploration point.

When `BufferManager` is initialized, it automatically creates one `RootAudioBuffer` per audio channel per token.
`RootAudioBuffer` works much the same way as `RootNode` where:

- It serves as the central registry of buffers, their processors and their chained processors, per channel.
- It is responsible for processing the default processors of all registered buffers and their processing chains
- It handles accumulation of the processed output of all registered buffers
- It mixes any node output registered to that specific channel
- It handles normalization, applying limiter algorithm and clipping bounds of the final output

Buffers can be directly added to a `RoodAudioBuffer` via `manager->get_root_audio_buffer(token, channel)->add_child_buffer(buffer)`.
Similar to `RootNode`, a buffer is registered only when it is safe.

The default processor of the `RootAudioBuffer` handles most of the features listed above, whereas the `FinalProcessor` of handles limiting and normalizing.

### Direct buffer management and processing

Buffers and Processors can exist outside of the `BufferManager` context. `Buffer` is an interface class that `AudioBuffer` inherits from.
`auto buffer = std::make_shared<AudioBuffer>(0, 512);`

The only default property of concern is `default_processor`, which was introduced in the previous document. But that can be overridden with
`AudioBuffer::set_default_processor()`

Buffers also accept a `BufferProcessingChain` that allows attaching a series of `BufferProcessors` that evaluate in order of processor registration;

```cpp
auto proc = std::make_shared<FeedbackProcessor>)();
buffer->set_default_processor(proc);
// Same processor for multiple buffers
buf2->set_default_processor(proc);
buffer->set_processing_chain(processing_chain);

// Process default and after 2 seconds process chain
MayaFlux::schedule_sequence({
    {2, [buffer](){
       buffer->process_default();
    }},
    {3, [buffer, processing_chain](){
       processing_chain->process(buffer);
    }},
    {1, [buf2, processing_chain](){
       // Same processing chain can process multiple buffers in any order
       processing_chain->process(buf2);
    }}
});
```

Sharing data between buffers can still be accommodated outside of `BufferManager`.

```cpp
auto feed_buf = std::make_shared<FeedbackBuffer>(0, 512);
// Creates a clone of the current buffer with different channel ID.
// shares the default processor and the processing chain (evaluated independently without interference)
auto new_buf = feed_buf->clone_to(1);
// Share data between buffers;
new_buf->read_once(feed_buf);
```

The methods for extending processors themselves was introduced in the previous document, so its skipped here.

## Coroutines

Temporal coordination in MayaFlux operates through two interconnected namespaces: `Vruta` (scheduling infrastructure) and `Kriya` (creative temporal patterns). `Engine` manages coroutine coordination through `TaskScheduler`, similar to how it handles nodes and buffers.

### TaskScheduler

The Engine provides central lifecycle management for coroutines via `TaskScheduler`, which coordinates temporal processing across different domains

```cpp
// Fluent API - Engine handles domain assignment and task registration
auto shape_node = vega.Polynomial({0.1, 0.5, 2.f});
auto coordination_routine = shape_node >> Time(2.f);  // Automatic temporal domain assignment

// API Wrappers - Engine manages task registration and token assignment
MayaFlux::schedule_metro(2.0, []() {
    modulate_filter_cutoff();
}, "main_clock");  // Auto-registered to default SAMPLE_ACCURATE token

MayaFlux::schedule_pattern([](uint64_t beat) {
    return beat % 8 == 0;  // Every 8th beat
}, []() {
    change_distribution();
}, 1.0, "pattern_trigger");  // Auto-assigned timing characteristics

// Engine handles cross-domain coordination
auto event_chain = MayaFlux::create_event_chain()
    .then([]() { start_clock(); }, 0.0)
    .then([]() { trigger_buffer_copy(); }, 0.1)
    .then([]() { start_input_capture(); }, 0.5);
```

`TaskScheduler`'s responsibilities:

- Assigns processing tokens based on temporal precision requirements
- Registers coroutines with appropriate domain clocks (SampleClock, FrameClock, etc.)
- Manages task hierarchies across different processing tokens
- Coordinates timing and synchronization across multiple temporal domains
- Allows safe access to data stored in coroutine frame
- Exposes control over coroutine state schedule.

When using `MayaFlux::schedule_metro`, using internal awaiter `SampleDelay` it constructs a `Vruta::SoundRoutine` frame -> store it in a shared_ptr, calls `TaskScheduler::add_task` which extracts token based on awaiter and adds it to the graph.

When using temporal fluent operations like `node >> Time(2.f)`, the instruction creates a coroutine -> gets default `TaskScheduler` from engine -> registers it for the appropriate domain -> implicitly creates `Kriya::NodeTimer` and registers one-shot time operation;

### Kriya

`Kriya` namespaces contains a variety of coroutine designs for fluent and expressive usage of coroutines, beyond simple timing orchestration.

`Kriya::metro`, `Kriya::schedule`, `Kriya::pattern` and `Kriya::line` have already been introduced previously, which need not be created using API wrappers such as `MayaFlux::schedule_metro`.

However, unlike `Nodes` and `Buffers` they have no `process` callback, their procedure and state management are orchestrated by internal clock mechanism (Read, for more information).

`Kriya` also exposes one-shot timers, timed events, timed data capture mechanisms

```cpp
 // Create a timer
 Timer timer(*scheduler);
 // Schedule a callback to execute after 2 seconds
 timer.schedule(2.0, []() {
     std::cout << "Two seconds have passed!" << std::endl;
 });

 // Create a timed action
 TimedAction action(*scheduler);
 // Execute an action that lasts for 3 seconds
 action.execute(
     []() { std::cout << "Starting action" << std::endl; },
     []() { std::cout << "Ending action" << std::endl; },
     3.0
 );

 // Create a node timer
 NodeTimer timer(*scheduler, *graph_manager);
 timer.play_for(process_node, 2.0);

 // Automate buffer operations by capturing data
 auto capture_op = CaptureBuilder(audio_buffer)
     .for_cycles(10)
     .with_window(512, 0.5f)
     .on_data_ready([](const auto& data, uint32_t cycle) {
         process_windowed_data(data, cycle);
     })
     .with_tag("spectral_analysis");

 pipeline >> capture_op >> route_to_container(output_stream);
```

Each of these operations allow expressive routing of not just data but also procedure. And the nature MayaFlux's coroutine frame allows wrapping any coroutine inside recursive coroutines.
Each of the methods already wrap different coroutines based on the chained operation

### Clock Systems

When a coroutine is registered to a domain in TaskScheduler, it operates within that domain's `Clock` system. The actual clock implementations are:

```cpp
// TaskScheduler automatically creates clocks based on processing tokens
auto scheduler = MayaFlux::get_scheduler();

// Access domain-specific clocks
const auto& sample_clock = scheduler->get_clock(Vruta::ProcessingToken::SAMPLE_ACCURATE);
const auto& frame_clock = scheduler->get_clock(Vruta::ProcessingToken::FRAME_ACCURATE);

// Clock operations
auto current_sample = sample_clock.current_position();
auto current_time = sample_clock.current_time();
auto sample_rate = sample_clock.rate();

// Manual temporal advancement through scheduler
scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 1024);
```

Clock systems expose `tick(units)`, `current_position()`, `current_time()`, `rate()`, and `reset()`. The TaskScheduler's `process_token()` method handles temporal state advancement, processing unit calculation, and coroutine suspension/resumption coordination.

Clocks are automatically created when a processing token is first used through the `ensure_domain()` method:

```cpp
// Custom scheduler with explicit rates
auto scheduler = std::make_shared<Vruta::TaskScheduler>(48000, 60);

// Clocks created automatically when tokens are used
scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 1024);
scheduler->process_token(Vruta::ProcessingToken::FRAME_ACCURATE, 1);

// Access timing information
auto sample_units = scheduler->seconds_to_samples(2.0);  // Convert to samples
auto current_units = scheduler->current_units(Vruta::ProcessingToken::SAMPLE_ACCURATE);
```

### Direct Coroutine Management

#### Self-managed SoundRoutine Creation

Coroutines can be created directly using the `SoundRoutine` API and managed through the TaskScheduler:

```cpp
// Create SoundRoutine using API-based awaiters
auto temporal_pattern = [](Vruta::TaskScheduler& scheduler) -> Vruta::SoundRoutine {
    auto& promise = co_await Kriya::GetAudioPromise{};

    while (true) {
        // Check termination flag set by external control
        if (promise.should_terminate) {
            break;
        }

        // Sample-accurate delay
        co_await Kriya::SampleDelay{scheduler.seconds_to_samples(0.5)};
        pulse_node->process_sample();

        // Access and modify promise state
        promise.set_state("frequency", 440.0f);
        auto current_freq = promise.get_state<float>("frequency");
    }
};

// Create and manage routine
auto routine = std::make_shared<Vruta::SoundRoutine>(temporal_pattern(*scheduler));
scheduler->add_task(routine, "temporal_pattern");

// Manual processing control
MayaFlux::schedule_metro(0.02, [=]() {
    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
                           scheduler->seconds_to_samples(0.02));
});
```

#### API-based Awaiter Patterns

The actual awaiter implementations available for coroutine control:

```cpp
// Using pre-built coroutine patterns with API awaiters
auto metro_routine = [](Vruta::TaskScheduler& scheduler) -> Vruta::SoundRoutine {
    auto& promise = co_await Kriya::GetAudioPromise{};
    uint64_t interval_samples = scheduler.seconds_to_samples(2.0);

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        // Execute callback
        modulate_filter_cutoff();

        // Wait for next beat
        co_await Kriya::SampleDelay{interval_samples};
    }
};
```

### Direct Routine Control and State Management

```cpp
// Create routine with state management
auto stateful_routine = [](Vruta::TaskScheduler& scheduler) -> Vruta::SoundRoutine {
    auto& promise = co_await Kriya::GetAudioPromise{};

    // Initialize state
    promise.set_state("amplitude", 0.8f);
    promise.set_state("frequency", 440.0f);

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        // Access state
        auto amplitude = promise.get_state<float>("amplitude");
        auto frequency = promise.get_state<float>("frequency");

        // Update processing based on state
        sine_node->set_frequency(*frequency);
        sine_node->set_amplitude(*amplitude);

        co_await Kriya::SampleDelay{scheduler.seconds_to_samples(0.01)};
    }
};

// External routine control
auto routine = std::make_shared<Vruta::SoundRoutine>(stateful_routine(*scheduler));
scheduler->add_task(routine, "stateful_process");

// Update routine parameters externally
routine->update_params("amplitude", 0.5f, "frequency", 880.0f);

// Control routine lifecycle
routine->set_should_terminate(true);  // Stop routine
routine->restart();                   // Restart from beginning
```

### Multi-domain Coroutine Coordination

```cpp
// Multi-rate coroutine for cross-domain coordination
auto sync_routine = [](Vruta::TaskScheduler& scheduler) -> Vruta::ComplexRoutine {
    auto& promise = co_await Kriya::GetAudioPromise{};

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        // Audio-rate processing
        co_await Kriya::SampleDelay{scheduler.seconds_to_samples(0.02)};
        process_audio_frame();

        // Frame-rate processing (when graphics system is ready)
        // co_await Kriya::FrameDelay{1};
        // update_visual_frame();

        // Multi-rate delay combining both domains
        co_await Kriya::MultiRateDelay{
            .samples_to_wait = scheduler.seconds_to_samples(0.1),
            .frames_to_wait = 6  // ~6 frames at 60fps
        };
    }
};
```

---

This architecture enables computational thinking as creative expression—where the choice between automatic coordination and explicit control becomes part of the creative decision-making process.

Domain composition allows creators to think in terms of unified computational environments while maintaining the flexibility to optimize for specific creative requirements.

For advanced and architecture level presentation of the same topic, please refer to [Advanced Context Control](Advanced_Context_Control.md)
