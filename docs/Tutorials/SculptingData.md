# Tutorial: Sculpting Data

In MayaFlux, data isn’t something you use -> it’s something you shape.
Sound, light, numbers, all share the same substrate.
You don’t generate a waveform; you sculpt a pattern of information and let it move.
These tutorials begin with the smallest gesture (loading a file) and expand until you can construct entire temporal architectures.
The point is not playback. The point is agency over time.

Every example you run produces real sound, but the goal is not sound itself — the goal is to understand the movement of information.

Each section in this series introduces one idea:
- A way of structuring data
- A way of scheduling time
- A way of controlling how information flows

Together, they form the foundation of digital composition — not in the musical sense, but in the computational one.

What you’ll do here:
- Load data and inspect its structure
- Connect it to buffers and observe flow
- Insert processors and shape transformations
- Learn how timing and cycles define motion

Eventually, build declarative pipelines that describe complete computational events

What you won’t do here:
- Build UIs or “patches”
- Work through effects lists or presets
- Simulate analog workflows

Everything here is real code: The same logic that runs inside the MayaFlux engine.
You’ll read it, modify it, and run it directly.

Each step is designed to teach you how the system thinks, so that later, when you invent something new, 
you can do so fluently without waiting for someone else to provide the building blocks.

## The Simplest First Step

Run this code. The file is loaded into memory.

```cpp
// In your src/user_project.hpp compose() function:

void compose() {
    auto sound_container = vega.read("path/to/your/file.wav");
}
```

Replace `"path/to/your/file.wav"` with an actual path to a `.wav` file.

Run the program. You'll see console output showing what loaded:
```
✓ Loaded: path/to/your/file.wav
  Channels: 2
  Frames: 2304000
  Sample Rate: 48000 Hz
```

Nothing plays yet. That's intentional—and important. The rest of this section shows you what just happened.

You have:
- All audio data in memory
- Organized as a Container with metadata
- A processor attached, ready to chunk and feed data
- Full control over what happens next

The file is loaded. Ready. Waiting.

---

## Expansion 1: What Is a Container?

<details>
<parameter name="open">

When you call `vega.read()`, you're not just reading bytes from disk and forgetting them. You're creating a **Container**—a structure that holds:

- **The audio data itself** (all samples as numbers, deinterleaved and ready to process)
- **Metadata about the data** (sample rate, channels, duration, number of frames)
- **A processor** (machinery that knows how to access this data efficiently)
- **Organizational structure** (dimensions: time, channels, memory layout)

The difference: A file is **inert**. A Container is **active creative material**. It knows its own shape.
It can tell you about regions within itself. It can be queried, transformed, integrated with other Containers.

When `vega.read("file.wav")` runs, MayaFlux:
1. Creates a `SoundFileReader` and initializes FFmpeg
2. Checks if the file is readable
3. Resamples to your project's sample rate (configurable)
4. Converts to 64-bit depth (high precision)
5. **Deinterleaves** the audio (separates channels into independent arrays—more efficient for processing)
6. Creates a `SoundFileContainer` object
7. Loads all the audio data into memory
8. Configures a `ContiguousAccessProcessor` (the Container's default processor, which knows how to feed data to buffers chunk-by-chunk)
9. Returns the Container to you

The Container is now your interface to that audio data. It's ready to be routed, processed, analyzed, transformed.

</details>

---

## Expansion 2: Memory, Ownership, and Smart Pointers

<details>
<parameter name="open">

As you know, raw audio data can be large. MayaFlux allocates and manages it safely through smart pointers.

At a lower, machine-level (in programming parlance), the user is expected to manage memory manually:
instantiate objects, bind them, handle transfers, and delete when done.
Any misalignment among these steps can cause crashes or undefined behavior.
MayaFlux doesn’t expect you to handle these manually—unless you choose to.

MayaFlux uses **smart pointers**—a C++11 feature that automatically tracks how many parts of your program are using a Container. 
When the last reference disappears, the memory is freed automatically.

When you write:
```cpp
auto sound_container = vega.read("file.wav");
```

What's actually happening is:
```cpp
std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> sound_container = 
    /* vega.read() internally creates and returns a shared_ptr */;
```

You don't see `std::shared_ptr`. You see `auto`. But MayaFlux is using it. This means:

- **You never manually `delete` the Container.** It handles itself.
- **Multiple parts of your code can reference the same Container** without worrying about who's responsible for cleanup.
- **When the last reference is gone**, memory is automatically released.

This is why `vega.read()` is safe. The complexity of memory management exists—it's just not your problem.

</details>

---

## Expansion 3: What is `vega`?

<details>
<parameter name="open">

`vega` is a **fluent interface**—a convenience layer that takes MayaFlux's power and hides the verbosity without hiding the machinery.
Grappling with complexity generally yields expressive, and often well-reasoned, implementations.
However, many find it hard to parse the wall of code that results from such grappling,
partly because machine-level languages tend to prioritize other aspects of coding over user experience (UX).

Making complex logic less verbose can be a good way to encourage more people to explore.

If you didn't have `vega`, loading a file would look like this:

```cpp
// Without vega - explicit, showing every step
auto reader = std::make_unique<MayaFlux::IO::SoundFileReader>();
MayaFlux::IO::SoundFileReader::initialize_ffmpeg();

if (!reader->can_read("file.wav")) {
    std::cerr << "Cannot read file\n";
    return;
}

reader->set_target_sample_rate(MayaFlux::Config::get_sample_rate());
reader->set_target_bit_depth(64);
reader->set_audio_options(MayaFlux::IO::AudioReadOptions::DEINTERLEAVE);

MayaFlux::IO::FileReadOptions options = MayaFlux::IO::FileReadOptions::EXTRACT_METADATA;
if (!reader->open("file.wav", options)) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO, "Failed to open file: {}", reader->get_last_error());
    return;
}

auto container = reader->create_container();
auto sound_container = std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(container);

if (!reader->load_into_container(sound_container)) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime, "Failed to load audio data: {}", reader->get_last_error());
    return;
}

auto processor = std::dynamic_pointer_cast<Kakshya::ContiguousAccessProcessor>(
    sound_container->get_default_processor());
if (processor) {
    std::vector<uint64_t> output_shape = { 
        MayaFlux::Config::get_buffer_size(), 
        sound_container->get_num_channels() 
    };
    processor->set_output_size(output_shape);
    processor->set_auto_advance(true);
}

// Now you have sound_container
```

Depending on your exposure to programming, this can either feel complex or liberating.
Lacking the facilities to be explicit about memory management or allocation can be limiting:
- Not knowing when memory is created, bound, or cleared
- Realizing too late that your memory usage is exceeding the budget
- Slowing the system for the false simplicity of “available without effort”
These often lead to confinement and confusion.

However, the above code snippet is verbose for something so simple.

`vega` says: "You just want to load a file? Say so."

```cpp
auto sound_container = vega.read("file.wav");
```

Same machinery underneath. Same FFmpeg integration. Same resampling. Same deinterleaving. Same processor setup. Same safety.

**What `vega` does:**
- Infers format from filename extension
- Initializes the reader with sensible defaults
- Handles error checking internally
- Constructs the Container correctly
- Configures the processor
- Returns the result

**What `vega` doesn't do:**
- Hide the complexity. It subsumes the *verbosity*, not the *idea*.
- Make the Container less capable. It's the full Container with all features.
- Remove your ability to do this explicitly. You can always write the long version if you need control.

The short syntax is convenience. The long syntax is control. MayaFlux gives you both.

</details>

Use `vega` because you value fluency, not because you fear the machinery.

---

## Expansion 4: The Container's Processor

<details>
<parameter name="open">

The Container you just created isn't just a data holder. 
It has a **default processor**—a piece of machinery attached to it that knows how to feed data to buffers.

This processor (`ContiguousAccessProcessor`) does crucial work:

1. **Understands the memory layout** - how the Container's audio data is organized
2. **Knows the buffer size** - how many samples to chunk at a time (typically 512 or 4096)
3. **Tracks position** - where in the file you are (auto-advance means it moves forward each time data is requested)
4. **Deinterleaves access** - gives channels separately (crucial for processing, as you can transform each channel independently)

When you later connect this Container to buffers (in the next section), the processor is what actually feeds the data—it’s the active mechanism.

`vega.read()` configures this processor automatically:
- Sets output size to your project's buffer size
- Enables auto-advance (keeps moving through the file)
- Registers it with the Container

This is why `StreamContainers` (that `SoundFileContainer` inherits from) are more than data—they're *active*,
with built-in logic for how they should be consumed.

</details>

---

## Expansion 5: What `.read()` Does NOT Do

<details>
<parameter name="open">

This is important:

**`.read()` does NOT:**
- Start playback
- Create buffers
- Connect to your audio hardware
- Route data anywhere

**`.read()` DOES:**
- Read file from disk
- Decode audio (handle any format: WAV, MP3, FLAC, etc. via FFmpeg)
- Resample to your project's sample rate
- Allocate memory for all samples
- Deinterleave channels
- Attach a processor that knows how to access this data
- Return you a Container

The Container sits in memory, ready to be used. But “ready to be used” means you decide what happens next:
process it, analyze it, route it to output or visual processing, feed it into a machine-learning pipeline, anything.

**That’s the power of this design**: loading is separate from routing. You can load a file and immediately send it to hardware,
or spend the next 20 lines building a complex processing pipeline before ever playing it.

</details>

---

In the next section, we'll connect this Container to buffers and route it to your speakers.
And you'll see why this two-step design—load, then connect—is more powerful than one-step automatic playback.

---

# Tutorial: Connect to Buffers

## The Next Step

You have a Container loaded. Now you need to send it somewhere.

```cpp
auto sound_container = vega.read("path/to/file.wav");
auto buffers = MayaFlux::hook_sound_container_to_buffers(sound_container);
```

Run this code. Your file plays.

The Container + the hook call—together they form the path from disk to speakers. This section shows you what that connection does.

---

## Expansion 1: What Are Buffers?

<details>
<summary>Click to expand: Understanding Buffers</summary>

A **Buffer** is a temporal accumulator—a space where data gathers until it's ready to be released, then it resets and gathers again.

Buffers don't store your entire file. They store chunks. At your project's sample rate (48 kHz),
a typical buffer might hold 512 or 4096 samples: a handful of milliseconds of audio.

Here's why this matters:

Your audio interface (speakers, headphones) has a fixed callback rate. It says:
"Give me 512 samples of audio, and do it every 10 milliseconds. Repeat forever until playback stops."

Buffers are the industry standard method to meet this demand.

1. **Gathers** - accumulates samples from your Container (via its processor)
2. **Holds** - keeps those samples temporarily
3. **Releases** - sends them to hardware
4. **Resets** - becomes empty and ready for the next chunk

This cycle repeats thousands of times per minute. Buffers make that possible.

Without buffers, you'd have to manually manage these chunks yourself. With buffers, MayaFlux handles the cycle. 
Your Container's processor feeds data into them. The buffers exhale it to your ears.

</details>

---

## Expansion 2: Why Per-Channel Buffers?

<details>
<summary>Click to expand: Stereo, Mono, and Channel Architecture</summary>

A stereo file has 2 channels. A multichannel file might have 4 or 8 channels. MayaFlux doesn't merge them into one buffer.

Instead, it creates **one buffer per channel**.

Why? Because channels are independent processing domains. A stereo file's left channel and right channel:

- Can be processed differently
- Can be routed to different outputs
- Can have different process chains
- Can be analyzed separately
- Can coordinate with each other without conflict

When you hook a stereo Container to buffers, MayaFlux creates:
- Buffer for channel 0 (left)
- Buffer for channel 1 (right)

Each buffer:
- Pulls samples from the Container's channel 0 or channel 1 (via the Container's processor)
- Gets filled with 512/4096/etc. samples
- Sends those samples to the audio interface's corresponding output

This per-channel design is why you can later insert processing on a per-channel basis.
Insert a filter on channel 0? The first channel gets filtered.
Leave channel 1 alone? The second channel plays unprocessed.
This flexibility is only possible because channels are architecturally separate at the buffer level.

</details>

---

## Expansion 3: The Buffer Manager and Buffer Lifecycle

<details>
<summary>Click to expand: How Buffers Are Created and Managed</summary>

MayaFlux has a **buffer manager**—a central system that creates, tracks, and coordinates all buffers in your program.

When you call `hook_sound_container_to_buffers()`, here's what happens:

```cpp
auto buffer_manager = MayaFlux::get_buffer_manager();
uint32_t num_channels = container->get_num_channels();

for (uint32_t channel = 0; channel < num_channels; ++channel) {
    auto container_buffer = buffer_manager->create_audio_buffer<ContainerBuffer>(
        ProcessingToken::AUDIO_BACKEND,
        channel,
        container,
        channel);
    container_buffer->initialize();
}
```

Step by step:

1. **Get the buffer manager** - a global system that owns all buffers
2. **Ask the Container: how many channels?** - determines the loop count
3. **For each channel:**
   - Create an audio buffer of type `ContainerBuffer` (a buffer that reads from a Container)
   - Tag it with `AUDIO_BACKEND` (more on this in Expansion 5)
   - Tell it which channel matrix the buffer should belongs to
   - Tell it which channel in the Container to read from
   - Initialize it (prepare it for the callback cycle)

Now the buffer manager knows:
- These buffers exist
- These buffers are tied to this Container
- These buffers should feed the audio hardware
- These buffers are ready to cycle

When the audio callback fires (every 10ms at 48 kHz), the buffer manager wakes up 
all its `AUDIO_BACKEND` buffers and says: "Time for the next chunk. Fill yourselves."

Each buffer asks its Container's processor: "Give me 512 samples from your channel."

The processor pulls from the Container, advances its position, and hands back a chunk.

The buffer receives it and passes it to the audio interface.

Repeat forever.

</details>

---

## Expansion 4: ContainerBuffer—The Bridge

<details>
<summary>Click to expand: How Buffers Know Their Source</summary>

You created a `ContainerBuffer`, not just a generic `Buffer`. Why the distinction?

A **Buffer** is abstract—it's a temporal accumulator. But abstract things don't know where their data comes from.

A **ContainerBuffer** is specific—it's a buffer that knows:
- "My data comes from a Container"
- "My Container has a processor that chunks data"
- "I ask that processor for samples from a specific channel"

When the callback fires, the ContainerBuffer doesn't generate samples. 
It asks: "Container, give me the next 512 samples from your channel 0."

The Container's processor (remember `ContiguousAccessProcessor` from Section 1?) handles this. It:
- Knows where in the file you are (it tracks position)
- Knows how much data to chunk (512 samples)
- Pulls that many samples from its memory
- Auto-advances (moves the position forward)
- Returns the chunk

The ContainerBuffer receives it. Done.

This is the architecture: **Buffers don't generate or transform. They request and relay.** 
The Container's processor does the work. The buffer coordinates timing with hardware.

Later, when you add processing nodes or attach processing chains, 
you'll insert them between the Container's output and the buffer's input.
The buffer still doesn't transform—it still just relays. But what it relays will have been processed first.

</details>

---

## Expansion 5: Processing Token—AUDIO_BACKEND

<details>
<summary>Click to expand: Tokens, Domains, and Hardware Destinations</summary>

In the buffer creation code:

```cpp
auto container_buffer = buffer_manager->create_audio_buffer<ContainerBuffer>(
    ProcessingToken::AUDIO_BACKEND,
    channel,
    container,
    channel);
```

Notice `ProcessingToken::AUDIO_BACKEND`. This is a **token**—a semantic marker that tells MayaFlux:

- **This buffer is audio-domain** (not graphics, not compute)
- **This buffer is connected to the hardware backend** (it's the final destination before speakers)
- **This buffer runs at audio callback rate** (every ~10ms at 48 kHz, every 512 samples)
- **This buffer synchronizes with the real-time audio clock**

Tokens are how MayaFlux coordinates different processing domains without confusion. Later, you might have:

- `AUDIO_BACKEND` buffers - connected to speakers (hardware real-time)
- `AUDIO_PARALLEL` buffers - internal processing (process chains, analysis, etc.)
- `GRAPHICS_BACKEND` buffers - visual domain (frame-rate, not sample-rate)

Each token tells the system what timing, synchronization, and backend this buffer belongs to.

For now: `AUDIO_BACKEND` means "this buffer is feeding your ears directly. It must keep real-time pace with the audio interface."

</details>

---

## Expansion 6: Accessing the Buffers

<details>
<summary>Click to expand: What You Can Do With the Buffers</summary>

When you call `vega.read() | Audio`, MayaFlux creates the buffers internally. But now, with the ability to get those buffers back, you have access to them:

```cpp
auto sound_container = vega.read("path/to/file.wav");
auto buffers = vega.get_last_created_container_buffers();

// Now you have the buffers as a vector:
// buffers[0] → channel 0
// buffers[1] → channel 1 (if stereo)
// etc.
```

Why is this useful? Because buffers own **processing chains**. And processing chains are where you'll insert 
processes, analysis, transformations - everything that turns passive playback into active processing.

Each buffer has a method:
```cpp
auto chain = buffers[0]->get_processing_chain();
```

This gives you access to the chain that currently handles that buffer's data. 
Right now, the chain just reads from the Container and writes to the hardware. 
But you can modify that chain. 
- Add processors. 
- Analyze data. 
- Route to different destinations.

This is the foundation for Section 3. You load a file, get the buffers, access their chains, and inject processing into those chains.

</details>
---

## The Fluent vs. Explicit Comparison

### Fluent (What happens behind the scenes)
```cpp
vega.read("path/to/file.wav") | Audio;
```

This single line does all of the above: creates a Container, creates per-channel buffers, hooks them to the audio hardware, 
and starts playback. No file plays until the `| Audio` operator, which is when the connection happens.

### Explicit (What's actually happening)
```cpp
auto sound_container = vega.read("path/to/file.wav");
auto buffers = vega.get_last_created_container_buffers();
// File is loaded, buffers exist, but no connection to hardware yet
// Buffers have chains, but nothing is using them

// To actually play, you'd need to ensure they're registered
// (vega.read() | Audio does this automatically)
```

**Understanding the difference:**
- The fluent version (`| Audio`) triggers buffer creation *and* hardware connection
- The explicit version gives you the buffers so you can inspect and modify them *before* hooking to hardware
- Both do the same thing—one is convenience, one is control

---

## Try It

```cpp
void compose() {
    vega.read("path/to/your/file.wav") | Audio;
    
    // File plays
}
```

Replace `"path/to/your/file.wav"` with an actual path.

You have:
- A Container loaded with all audio data (deinterleaved, resampled, ready)
- Per-channel buffers created, each tied to a Container channel
- Buffers registered with the buffer manager and audio interface
- The callback cycle running, continuously pulling chunks and feeding them to speakers
- Your file plays start-to-finish automatically

No code running during playback—just the callback cycle doing its work, thousands of times per minute.

In the next section, we'll modify these buffers' processing chains. We'll insert a filter processor and hear how it changes the sound.
This is where MayaFlux's power truly shines—transforming passive playback into active, real-time audio processing.

# Tutorial: Buffers Own Chains

## The Simplest Path

You have buffers. You can modify what flows through them.

```cpp
auto sound_container = vega.read("path/to/file.wav");
auto buffers = vega.get_last_created_container_buffers();

auto filter = vega.IIR({0.1, 0.2, 0.1}, {1.0, -0.6});
auto filter_processor = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[0], filter);
```

Run this code. Your file plays with a low-pass filter applied.

The filter smooths the audio—reduces high frequencies. Listen to the difference.

That's it. Three lines of code: load, get buffers, insert filter. The rest of this section shows you what just happened.

---

## Expansion 1: What Is `vega.IIR()`?

<details>
<summary>Click to expand: Creating Filter Nodes</summary>

`vega.IIR()` creates a filter node—a computation unit that processes audio samples one at a time.

An **IIR filter** (Infinite Impulse Response) is a mathematical operation that transforms samples based on feedback coefficients. The two parameters are:
- **Feedforward coefficients** `{0.1, 0.2, 0.1}` - how the current and past input samples contribute
- **Feedback coefficients** `{1.0, -0.6}` - how past output samples contribute

You don't need to understand the math. Just know: this creates a filter that smooths audio.

`vega` is the fluent interface—it subsumes verbosity. Without it:

```cpp
// Without vega - explicit
auto filter = std::make_shared<Nodes::Filters::IIR>(
    std::vector<double>{0.1, 0.2, 0.1},
    std::vector<double>{1.0, -0.6}
);
```

With vega:
```cpp
auto filter = vega.IIR({0.1, 0.2, 0.1}, {1.0, -0.6});
```

Same filter. Same capabilities. Vega just hides the verbosity.

</details>

---

## Expansion 2: What Is `MayaFlux::create_processor()`?

<details>
<summary>Click to expand: Wrapping Nodes in Processors</summary>

A **node** (like `vega.IIR()`) is a computational unit—it processes one sample at a time.

This **processor** is a buffer-aware wrapper around that node. It knows:
- How to extract data from a buffer
- How to feed samples to the node
- How to put the transformed samples back in the buffer
- How to handle the buffer's processing cycle

`create_processor()` wraps your filter node in a processor and attaches it to a buffer's processing chain.

```cpp
auto filter_processor = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[0], filter);
```

What this does:
1. Takes your filter node
2. Creates a `FilterProcessor` that knows how to apply that node to buffer data
3. Adds the processor to `buffers[0]`'s processing chain (implicit—this happens automatically)
4. Returns the processor so you can reference it later if needed

The buffer now has this processor in its chain. Each cycle, the buffer runs the processor, which applies the filter to all samples in that cycle.

</details>

---

## Expansion 3: What Is a Processing Chain?

<details>
<summary>Click to expand: How Buffers Execute Processors</summary>

Each buffer owns a **processing chain**—an ordered sequence of processors that transform data.

Your buffer's default processor was:
- **ContainerToBufferAdapter** - reads from the Container, fills the buffer

When `create_processor()` adds your FilterProcessor, the chain becomes:
1. Default processor: ContainerToBufferAdapter (reads from Container)
2. **FilterProcessor** (applies your filter) ← You just added this
3. Other processors you might add later (e.g., Writer to send to hardware)

Each cycle:
- Adapter fills the buffer with 512 samples from the Container
- FilterProcessor runs—modifies those 512 samples by applying the filter
- Other processors run in sequence

Data flows: **Container → [filtered] → Speakers**

The chain is ordered. Processors run in sequence. Output of one becomes input to next.

</details>

---

## Expansion 4: Adding Processor to Another Channel (Optional)

<details>
<summary>Click to expand: Multi-Channel Processing</summary>

Your stereo file has two channels. Right now, only channel 0 is filtered.

You can add the same processor to channel 1:

```cpp
auto filter = vega.IIR({0.1, 0.2, 0.1}, {1.0, -0.6});
auto fp0 = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[0], filter);
auto fp1 = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[1], filter);
```

Or more simply, add the existing processor to another buffer:

```cpp
auto filter = vega.IIR({0.1, 0.2, 0.1}, {1.0, -0.6});
auto filter_processor = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[0], filter);
MayaFlux::add_processor(filter_processor, buffers[1], MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND);
```

`add_processor()` adds an existing processor to a buffer's chain.

`create_processor()` creates a processor and adds it implicitly.

Both do the same underlying thing—they add the processor to the buffer's chain. `create_processor()` just combines creation and addition in one call.

Now both channels are filtered by the same IIR node. Different channel buffers can share the same processor or have independent ones—your choice.

</details>

---

## Expansion 5: What Happens Inside

<details>
<summary>Click to expand: The Machinery Under the Hood</summary>

When you call:
```cpp
auto filter_processor = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[0], filter);
```

MayaFlux does this:

```cpp
// 1. Create a new FilterProcessor wrapping your filter node
auto processor = std::make_shared<MayaFlux::Buffers::FilterProcessor>(filter);

// 2. Get the buffer's processing chain
auto chain = buffers[0]->get_processing_chain();

// 3. Add the processor to the chain
chain->add_processor(processor, buffers[0]);

// 4. Return the processor
return processor;
```

When `add_processor()` is called separately:
```cpp
MayaFlux::add_processor(filter_processor, buffers[1], MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND);
```

MayaFlux does this:

```cpp
// Get the buffer manager
auto buffer_manager = MayaFlux::get_buffer_manager();

// Get channel 1's buffer for AUDIO_BACKEND token
auto buffer = buffer_manager->get_buffer(ProcessingToken::AUDIO_BACKEND, 1);

// Get its processing chain
auto chain = buffer->get_processing_chain();

// Add the processor
chain->add_processor(processor, buffer);
```

The machinery is consistent: **processors are added to chains, chains are owned by buffers, buffers execute chains each cycle.**

You don't need to write this explicitly—the convenience functions handle it. But this is what's happening.

</details>

---

## Expansion 6: Processors Are Reusable Building Blocks

<details>
<summary>Click to expand: Composition and Flexibility</summary>

A processor is a building block. Once created, it can be:
- Added to multiple buffers (same processor, multiple channels)
- Composed with other processors (insert multiple processors)
- Swapped out (remove and replace)
- Queried (ask for its state, parameters, etc.)

Example: two channels with the same filter:
```cpp
auto filter = vega.IIR({0.1, 0.2, 0.1}, {1.0, -0.6});
auto processor = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[0], filter);
MayaFlux::add_processor(processor, buffers[1]);
```

Example: stacking processors (requires understanding of chains, shown later):
```cpp
auto filter1 = vega.IIR(...);
auto fp1 = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[0], filter1);

auto filter2 = vega.IIR(...); // Different filter
auto fp2 = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[0], filter2);
```

Now buffers[0] has two FilterProcessors in its chain. Data flows through both sequentially.

Processors are the creative atoms of MayaFlux. Everything builds from them.

</details>

---

## Try It

```cpp
void compose() {
    auto sound_container = vega.read("path/to/your/file.wav");
    auto buffers = vega.get_last_created_container_buffers();
    
    auto filter = vega.IIR({0.1, 0.2, 0.1}, {1.0, -0.6});
    auto filter_processor = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[0], filter);
}
```

Replace `"path/to/your/file.wav"` with an actual path.

Run the program. Listen. The audio is filtered.

Now try modifying the coefficients:
```cpp
auto filter = vega.IIR({0.05, 0.3, 0.05}, {1.0, -0.8});
```

Listen again. Different sound. You're sculpting the filter response.

You've just inserted a processor into a buffer's chain and heard the result. That's the foundation for everything that follows.

In the next section, we'll interrupt this passive playback. We'll insert a processing node between the Container and the buffers. 
And you'll see why this architecture—buffers as relays, not generators—enables powerful real-time transformation.

# Tutorial: Timing, Streams, and Bridges

## The Current Continous Flow

What you've done so far is simple and powerful:

```cpp
auto sound_container = vega.read("path/to/file.wav");
auto buffers = vega.get_last_created_container_buffers();
auto filter = vega.IIR({0.1, 0.2, 0.1}, {1.0, -0.6});
auto fp = MayaFlux::create_processor<MayaFlux::Buffers::FilterProcessor>(buffers[0], filter);
```

This flow is designed for **full-file playback**:
- Load the entire file into a Container
- route it through buffers
- add general purpose processes
- play to speakers (RtAudio backend via SubsystemManagers)

Clean. Direct. No timing control.

That's intentional.

There are other features—region looping, seeking, playback control,
but they don't fit this tutorial. These sections are purely for:
**file → buffers → output, uninterrupted.**

---

## Where We're Going

Here's what the next section enables:

```cpp
auto pipeline = MayaFlux::create_buffer_pipeline();
pipeline->with_strategy(ExecutionStrategy::PHASED); // Execute each phase fully before next op

pipeline
    >> BufferOperation::capture_file_from("path/to/file.wav", 0)  // From channel 0
    .for_cycles(20)  // Process 20 buffer cycles
    >> BufferOperation::transform([](auto& data, uint32_t cycle) {
        // Data now has 20 buffer cycles of audio from the file
        // i.e 20 x 512 samples if buffer size is 512
        auto zero_crossings = MayaFlux::zero_crossings(data);

        std::cout << "Zero crossings at indices:\n";
        for (const auto& sample : zero_crossings) {
            std::cout << sample << "\t";
        }
        std::cout << "\n";

        return data;
    });

pipeline->execute_buffer_rate();  // Schedule and run
```

This processes exactly 20 buffer cycles from the file (with any process you want), 
accumulates the result in a stream, and executes the pipeline.

The file isn't playing to speakers. It's being captured, processed, and stored in a stream. 
**Timing is under your control.**: You decide how many buffer cycles to process.
This section builds the foundation for buffer pipelines.
Understanding the architecture below explains why the code snippet works.

In this section, we will introduce the machinery for everything beyond simplicity. 
We're not building code that has audio yet. We're establishing the architecture 
that enables timing control, streaming, capture, and composition.

---

## Expansion 1: The Architecture of Containers

<details>
<summary>Click to expand: Why We Need Something Else</summary>

A Container (like SoundFileContainer) holds all data upfront:
- Load entire file into memory
- Data is fixed size
- Processor knows where in the file you are
- Designed for sequential access—read start, advance, read next chunk, repeat until end

This works perfectly for "play the whole file".
It also works for as yet unexpored controls over the same timeline,
such as looping, seeking positions, jumping to regions, etc.

But it doesn't work for:
- **Recording**: You don't know the final size upfront
- **Structuring**: You need to manipulate boundaries
- **Streaming**: Data arrives in chunks; size grows dynamically
- **Capture**: You want to save specific buffer cycles, not the whole file

For these use cases, you need a different data structure.

</details>

---

## Expansion 2: Enter DynamicSoundStream

<details>
<summary>Click to expand: A Container That Grows</summary>

A **DynamicSoundStream** is a child class of `SignalSourceContainer` much like
`SoundFileContainer` that we have been using. It has the same interface 
as `SoundFileContainer` (channels, frames, metadata, regions). 
But it has different semantics:

- **Dynamic size**: Starts small, grows as data arrives
- **Transient modes**: Can operate as a circular buffer (fixed size, overwrites old data)
- **Sequential writing**: Designed to accept data sequentially from processors
- **No inherent structure**: Unlike SoundFileContainer 
        (which knows "this is a file with a start and end"),
        DynamicSoundStream is just a growing reservoir of data.

Think of it as:
- **SoundFileContainer**: "I am this exact file, with this exact data"
- **DynamicSoundStream**: "I am a space where audio data accumulates. I don't know how much will arrive."

DynamicSoundStream has powerful capabilities:
- **Auto-resize mode**: Grows as data arrives (good for recording)
- **Circular mode**: Fixed capacity, wraps around (good for delay lines or rolling analysis)
- **Position tracking**: Knows where reads/writes are in the stream
- **Capacity pre-allocation**: You can reserve space if you know approximate size

You don't create DynamicSoundStream directly (yet). It's managed implicitly by other systems.
But understanding what it is explains everything that follows.

</details>

---

## Expansion 3: StreamWriteProcessor

<details>
<summary>Click to expand: Writing Buffer Data to Streams</summary>

You've seen `BufferProcessors` like `FilterProcessor` that transform data in place.

But `StreamWriteProcessor` is more general. It can write buffer data to **any** `DynamicSoundStream`, 
not just locally to attached buffers (or from hardware: hitherto unexplored `InputListenerProcessor`).

When a processor runs each buffer cycle:
1. Buffer gets filled with 512 samples (from Container or elsewhere)
2. Processors run (your `FilterProcessor`, for example)
3. `StreamWriteProcessor` writes the (now-processed) samples to a `DynamicSoundStream`

The `DynamicSoundStream` accumulates these writes:
- Cycle 1: 512 samples written
- Cycle 2: Next 512 samples written (total: 1024)
- Cycle 3: Next 512 samples written (total: 1536)
- ...

After N cycles, the `DynamicSoundStream` contains N × 512 samples of processed audio.

This is how you capture buffer data. Not by sampling the buffer once,
by continuously writing it to a stream through a processor.

`StreamWriteProcessor` is the bridge between buffers (which live in real-time) and streams (which accumulate).

</details>

---

## Expansion 4: FileBridgeBuffer—Controlled Flow

<details>
<summary>Click to expand: The Reading-Writing Bridge</summary>

**FileBridgeBuffer** is a specialized buffer that orchestrates reading from a file and writing to a stream,
with timing control through buffer cycles.

Internally, FileBridgeBuffer creates a processing chain:

```
SoundFileContainer (source file)
    ↓
ContainerToBufferAdapter (reads from file, advances position)
    ↓
[Your processors here: filters, etc.]
    ↓
StreamWriteProcessor (writes to internal DynamicSoundStream)
    ↓
DynamicSoundStream (accumulates output)
```

The key difference from your simple load/play flow:
- Instead of routing to hardware, data goes to a DynamicSoundStream
- You control **how many buffer cycles** run (e.g., "process 10 cycles of this file")
- After N cycles, the stream holds N × buffer_size samples of the processed result

FileBridgeBuffer represents: 
**"Read from this file, process through this chain, accumulate result in this stream, for exactly this many cycles."**

This gives you timing control. You don't play the whole file. You process exactly N cycles, then stop.

</details>

---

## Expansion 5: Why This Architecture?

<details>
<summary>Click to expand: Decoupling Reading, Processing, and Output</summary>

The architecture separates concerns:

- **Reading**: Done by ContainerToBufferAdapter (reads from SoundFileContainer in controlled chunks)
- **Processing**: Done by your custom processors
- **Writing**: Done by StreamWriteProcessor (writes results to DynamicSoundStream)
- **Accumulation**: Done by DynamicSoundStream (holds the result)

Each layer is independent:
- You can swap the reader (use a different Container)
- You can insert any number of processors
- You can swap the writer (write to hardware, to disk, to memory, to GPU)
- The stream is just a data holder—it doesn't care what filled it

This is why FileBridgeBuffer is powerful: it composes these layers without forcing you to wire them manually.

And it's why understanding this section matters:
**the next tutorial (BufferOperation) builds on top of this composition**, adding temporal coordination and pipeline semantics.

</details>

---

## Expansion 6: From File to Cycle

<details>
<summary>Click to expand: "Cycles" as Timing Units</summary>

A **cycle** is one complete buffer processing round:
- 512 samples from the source
- Processed through all processors
- Written to the destination stream

At 48 kHz, one cycle is 512 ÷ 48000 ≈ 10.67 milliseconds of audio.

When you say "process this file for 20 cycles," you mean:
- Run 20 iterations of: read 512 → process → write 512
- Result: 10,240 samples (≈ 213 ms of audio at 48 kHz)

Timing control is expressed in **cycles**, not time. This is intentional:
- Cycles are deterministic (you know exactly how much data will be processed)
- Cycles are aligned with buffer boundaries (no partial processing)
- Cycles decouple from hardware timing (no real-time constraints)

FileBridgeBuffer lets you say: "Process this file for exactly N cycles," then accumulate the result in a stream.

This is the foundation for everything BufferOperation does—it extends this cycle-based thinking to composition and coordination.

</details>

---

## The Three Key Concepts

At this point, understand:

1. **DynamicSoundStream**: A container that grows dynamically, can operate in circular mode, designed to accumulate data from processors

2. **StreamWriteProcessor**: The processor that writes buffer data sequentially to a DynamicSoundStream

3. **FileBridgeBuffer**: A buffer that creates a chain (reader → your processors → writer), and lets you control how many buffer cycles run

These three concepts enable timing control. You're no longer at the mercy of real-time callbacks. 
You can process exactly N cycles, accumulate results, and move on.

---

## Why This Section Has No Audio Code

This is intentional. The concepts here are essential, and expose the architecture behind everything that follows.
It is also a hint at the fact that modal output is not the only use case for MayaFlux.

- FileBridgeBuffer is too low-level—you'd create it manually, call setup_chain_and_processor(), manage cycles yourself
- DynamicSoundStream is too generic—without a driver, you'd just accumulate data with no purpose
- StreamWriteProcessor is just a piece—alone, it doesn't tell you how many cycles to run

The **next tutorial introduces BufferOperation**, which wraps these concepts into high-level, composable patterns:
- `BufferOperation::capture_file()` - wrap FileBridgeBuffer, accumulate N cycles, return the stream
- `BufferOperation::file_to_stream()` - connect file reading to stream writing, with cycle control
- `BufferOperation::route_to_container()` - send processor output to a stream

Once you understand FileBridgeBuffer, DynamicSoundStream, and cycle-based timing, BufferOperation will feel natural.
It's just syntactic sugar on top of this architecture.

For now: **internalize the architecture. The next section shows how to use it.**

---

## What You Should Internalize

- Containers hold data (SoundFileContainer holds files; DynamicSoundStream holds growing data)
- Processors transform data (your FilterProcessor, StreamWriteProcessor, etc.)
- Buffers orchestrate cycles (read N cycles, run processors, write results)
- Streams accumulate (DynamicSoundStream holds results after cycles complete)
- Timing is expressed in cycles (deterministic, aligned with buffer boundaries, decoupled from real-time)

This is the mental model for everything that follows. Pipelines, capture, routing—they all build on this foundation.

# Tutorial: Buffer Pipelines (Teaser)

## The Next Level

Everything you've learned so far processes data in isolation: load a file, add a processor, output to hardware.

But what if you want to:
- **Capture** a specific number of buffer cycles from a file
- **Process** those cycles through custom logic
- **Route** the result to a buffer for playback
- **Do all of this in one declarative statement**

That's what buffer pipelines do.

---

## A Taste

```cpp
void compose() {
    // Create an empty audio buffer (will hold captured data)
    auto capture_buffer = vega.AudioBuffer()[1] | Audio;
    // Create a pipeline
    auto pipeline = MayaFlux::create_buffer_pipeline();
    // Set strategy to streaming (process as data arrives)
    pipeline->with_strategy(ExecutionStrategy::STREAMING);
    
    // Declare the flow: 
    pipeline
        >> BufferOperation::capture_file_from("path/to/audio/.wav", 0)
               .for_cycles(1) // Essential for streaming
        >> BufferOperation::route_to_buffer(capture_buffer) // Route captured data to our buffer
        >> BufferOperation::modify_buffer(capture_buffer, [](std::shared_ptr<AudioBuffer> buffer) {
            for (auto& sample : buffer->get_data()) {
                sample *= MayaFlux::get_uniform_random(-0.5, 0.5); // random "texture" between 0 and 0.5
            }
        });

    // Execute: runs continuously at buffer rate
    pipeline->execute_buffer_rate();
}
```

Run this. You'll hear the file play back at with noisy texture.
But the file never played to speakers directly: 
it was captured, processed, accumulated, then routed.

---

## Expansion 1: What Is a Pipeline?

<details>
<summary>Click to expand: Declarative Processing Chains</summary>

A **pipeline** is a declarative sequence of buffer operations that compose to form a complete computational event.

Unlike the previous sections where you manually:
1. Load a file
2. Get buffers
3. Create processors
4. Add to chains

...a pipeline lets you describe the entire flow in one statement:

```cpp
pipeline
    >> Operation1
    >> Operation2
    >> Operation3;
```

The `>>` operator chains operations. The pipeline executes them in order, handling all the machinery (cycles, buffer management, timing) invisibly.

This is why you've been learning the foundation first: **pipelines are just syntactic sugar over FileBridgeBuffer, DynamicSoundStream, StreamWriteProcessor, and buffer cycles.**

Understanding the previous sections makes this section obvious. You're not learning new concepts—you're composing concepts you already understand.

</details>

---

## Expansion 2: BufferOperation Types

<details>
<summary>Click to expand: What Operations Exist</summary>

BufferOperation is a toolkit. Common operations:

- **capture_file()** - Read N cycles from a file, accumulate in internal stream
- **modify_buffer()** - Apply custom logic to a specific AudioBuffer
- **route_to_buffer()** - Send accumulated result to an AudioBuffer for playback
- **route_to_container()** - Send result to a DynamicSoundStream (for recording, analysis, etc.)
- **transform()** - Map/reduce on accumulated data (structural transformation)
- **dispatch()** - Execute arbitrary code with access to the data

Each operation is a building block. Pipeline chains them together.

The full set of operations is the subject of its own tutorial. This section just shows the pattern.

</details>

---

## Expansion 3: The `on_capture_processing` Pattern

<details>
<summary>Click to expand: Processing Each Cycle</summary>

Notice in the example:

```cpp
>> BufferOperation::modify([](auto& data, uint32_t cycle) {
    // Called every cycle as data accumulates
    for (auto& sample : data) {
        sample *= 0.5;
    }
})
```

The `modify` operation runs **each cycle**—meaning:
- Cycle 1: 512 samples captured, modified by your lambda
- Cycle 2: Next 512 samples captured, modified
- Cycle 3: And so on

This is `on_capture_processing`: your custom logic runs as data arrives, not automated by external managers.

Automatic mode simply expects buffer manager to handle the processing of attached processors.
On Demand mode expects users to provide callback timing logic.

For now: understand that pipelines let you hook custom logic into the capture/process/route flow.

</details>

---

## Expansion 4: Why This Matters

<details>
<summary>Click to expand: Composability and Control</summary>

Before pipelines, your workflow was:
1. Load file (Container)
2. Get buffers
3. Add processors to buffers
4. Play to hardware
5. Everything was real-time

With pipelines, your workflow is:
1. Declare capture (file, cycle count)
2. Declare processing (what to do each cycle)
3. Declare output (where result goes)
4. Execute (all at once, deterministic, no real-time constraints)

The key difference: **determinism**. You know exactly what will happen because you've declared the entire flow.

This is the foundation for everything beyond this tutorial:
- Recording sessions
- Batch processing
- Data analysis pipelines
- Complex temporal arrangements
- Multi-file composition

All of it starts with this pattern: **declare → execute → observe**.

</details>

---

## What Happens Next

The full **Buffer Pipelines** tutorial is its own comprehensive guide. It covers:
- All BufferOperation types
- Composition patterns (chaining operations)
- Timing and cycle coordination
- Error handling and introspection
- Advanced patterns (branching, conditional operations, etc.)

This section is just the proof-of-concept: "Here's what becomes possible when everything you've learned composes."

---

## Try It (Optional)

The code above will run if you have:
- A `.wav` file at `"path/to/file.wav"`
- All the machinery from Sections 1-3 understood

If you want to experiment, use a real file path and run it.

But the main point is: **understand what's happening**, not just make it work.

- You're capturing 50 buffer cycles (≈ 500ms of audio at 48 kHz)
- Each cycle, your lambda processes 512 samples
- Results accumulate in capture_buffer
- Then capture_buffer plays to hardware

This is real composition. Not playback. Not presets. Declarative data transformation.

---

## The Philosophy

You've now seen the complete stack:
1. **Containers** hold data (load files)
2. **Buffers** coordinate cycles (chunk processing)
3. **Processors** transform data (effects, analysis)
4. **Chains** order processors (sequence operations)
5. **Pipelines** compose chains (declare complete flows)

Each layer builds on the previous. None is magic. All are composable.

This is how MayaFlux thinks about computation: as layered, declarative, composable building blocks.

Pipelines are where that thinking becomes powerful. They're not a special feature—they're just the final layer of composition.

---

## Next: The Full Pipeline Tutorial

When you're ready, the standalone **"Buffer Pipelines"** tutorial dives deep into:
- Every BufferOperation type with examples
- How to compose complex workflows
- Error handling and debugging
- Performance considerations
- Real-world use cases

For now: you've seen the teaser. Everything you've learned so far is the foundation for that depth.

You understand how information flows. Pipelines just let you declare that flow elegantly.
