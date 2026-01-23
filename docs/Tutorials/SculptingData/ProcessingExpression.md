Up to this point, you’ve learned how audio flows:

- containers feed buffers
- buffers run processors
- processors shape data.

Now we expand the vocabulary of processors themselves.
In MayaFlux, mathematics, logic, feedback, and generation are not side features,
they are first-class sculpting tools. This tutorial explores how computational
expressions become sound-shaping primitives.

_In MayaFlux, polynomials don't calculate—they sculpt. Logic doesn't branch—it decides.
This tutorial shows you how mathematical expressions become sonic transformations._

---

- [Tutorial: Polynomial
  Waveshaping](#tutorial-polynomial-waveshaping){#toc-tutorial-polynomial-waveshaping}
  - [The Simplest Path](#the-simplest-path){#toc-the-simplest-path}
  - [Expansion 1: Why Polynomials Shape
    Sound](#expansion-1-why-polynomials-shape-sound){#toc-expansion-1-why-polynomials-shape-sound}
  - [Expansion 2: What `vega.Polynomial()`
    Creates](#expansion-2-what-vega.polynomial-creates){#toc-expansion-2-what-vega.polynomial-creates}
  - [Expansion 3:
    PolynomialMode::DIRECT](#expansion-3-polynomialmodedirect){#toc-expansion-3-polynomialmodedirect}
  - [Expansion 4: What `create_processor()`
    Does](#expansion-4-what-create_processor-does){#toc-expansion-4-what-create_processor-does}
  - [Try It](#try-it){#toc-try-it}
  - [Tutorial: Recursive Polynomials (Filters and
    Feedback)](#tutorial-recursive-polynomials-filters-and-feedback){#toc-tutorial-recursive-polynomials-filters-and-feedback}
    - [The Next Step](#the-next-step){#toc-the-next-step}
  - [Expansion 1: Why This Is a
    Filter](#expansion-1-why-this-is-a-filter){#toc-expansion-1-why-this-is-a-filter}
  - [Expansion 2: The History
    Buffer](#expansion-2-the-history-buffer){#toc-expansion-2-the-history-buffer}
  - [Expansion 3: Stability
    Warning](#expansion-3-stability-warning){#toc-expansion-3-stability-warning}
  - [Expansion 4: Initial
    Conditions](#expansion-4-initial-conditions){#toc-expansion-4-initial-conditions}
  - [Try It](#try-it-1){#toc-try-it-1}
- [Tutorial: Logic as Decision
  Maker](#tutorial-logic-as-decision-maker){#toc-tutorial-logic-as-decision-maker}
  - [The Simplest Path](#the-simplest-path-1){#toc-the-simplest-path-1}
  - [Expansion 1: What Logic
    Does](#expansion-1-what-logic-does){#toc-expansion-1-what-logic-does}
  - [Expansion 2: Logic node needs an
    input](#expansion-2-logic-node-needs-an-input){#toc-expansion-2-logic-node-needs-an-input}
  - [Expansion 3: LogicOperator
    Types](#expansion-3-logicoperator-types){#toc-expansion-3-logicoperator-types}
  - [Expansion 4: ModulationType - Readymade
    Transformations](#expansion-4-modulationtype---readymade-transformations){#toc-expansion-4-modulationtype---readymade-transformations}
  - [Try It](#try-it-2){#toc-try-it-2}
- [Tutorial: Combining Polynomial +
  Logic](#tutorial-combining-polynomial-logic){#toc-tutorial-combining-polynomial-logic}
  - [The Pattern](#the-pattern){#toc-the-pattern}
  - [Expansion 1: Decision Trees in
    Audio](#expansion-1-decision-trees-in-audio){#toc-expansion-1-decision-trees-in-audio}
  - [Expansion 2: Chain Order
    Matters](#expansion-2-chain-order-matters){#toc-expansion-2-chain-order-matters}
  - [Try It](#try-it-3){#toc-try-it-3}
- [Tutorial: Processing Chains and Buffer
  Architecture](#tutorial-processing-chains-and-buffer-architecture){#toc-tutorial-processing-chains-and-buffer-architecture}
  - [Tutorial: Explicit Chain
    Building](#tutorial-explicit-chain-building){#toc-tutorial-explicit-chain-building}
    - [The Simplest
      Path](#the-simplest-path-2){#toc-the-simplest-path-2}
  - [Expansion 1: What `create_processor()` Was
    Doing](#expansion-1-what-create_processor-was-doing){#toc-expansion-1-what-create_processor-was-doing}
  - [Expansion 2: Chain Execution
    Order](#expansion-2-chain-execution-order){#toc-expansion-2-chain-execution-order}
  - [Expansion 3: Default Processors vs. Chain
    Processors](#expansion-3-default-processors-vs.-chain-processors){#toc-expansion-3-default-processors-vs.-chain-processors}
  - [Try It](#try-it-4){#toc-try-it-4}
- [Tutorial: Various Buffer
  Types](#tutorial-various-buffer-types){#toc-tutorial-various-buffer-types}
  - [Generating from Nodes
    (NodeBuffer)](#generating-from-nodes-nodebuffer){#toc-generating-from-nodes-nodebuffer}
    - [The Next Pattern](#the-next-pattern){#toc-the-next-pattern}
    - [Expansion 1: What NodeBuffer
      Does](#expansion-1-what-nodebuffer-does){#toc-expansion-1-what-nodebuffer-does}
    - [Expansion 2: The `clear_before_process`
      Parameter](#expansion-2-the-clear_before_process-parameter){#toc-expansion-2-the-clear_before_process-parameter}
    - [Expansion 3: NodeSourceProcessor Mix
      Parameter](#expansion-3-nodesourceprocessor-mix-parameter){#toc-expansion-3-nodesourceprocessor-mix-parameter}
    - [Try It](#try-it-5){#toc-try-it-5}
  - [FeedbackBuffer (Recursive
    Audio)](#feedbackbuffer-recursive-audio){#toc-feedbackbuffer-recursive-audio}
    - [The Pattern](#the-pattern-1){#toc-the-pattern-1}
    - [Expansion 1: What FeedbackBuffer
      Does](#expansion-1-what-feedbackbuffer-does){#toc-expansion-1-what-feedbackbuffer-does}
    - [Expansion 2: FeedbackBuffer
      Limitations](#expansion-2-feedbackbuffer-limitations){#toc-expansion-2-feedbackbuffer-limitations}
    - [Expansion 3: When to Use
      FeedbackBuffer](#expansion-3-when-to-use-feedbackbuffer){#toc-expansion-3-when-to-use-feedbackbuffer}
    - [Try It](#try-it-6){#toc-try-it-6}
  - [SoundStreamWriter (Capturing
    Audio)](#streamwriteprocessor-capturing-audio){#toc-streamwriteprocessor-capturing-audio}
    - [The Pattern](#the-pattern-2){#toc-the-pattern-2}
    - [Expansion 1: What SoundStreamWriter
      Does](#expansion-1-what-streamwriteprocessor-does){#toc-expansion-1-what-streamwriteprocessor-does}
    - [Expansion 2: Channel-Aware
      Writing](#expansion-2-channel-aware-writing){#toc-expansion-2-channel-aware-writing}
    - [Expansion 3: Position
      Management](#expansion-3-position-management){#toc-expansion-3-position-management}
    - [Expansion 4: Circular
      Mode](#expansion-4-circular-mode){#toc-expansion-4-circular-mode}
    - [Try It](#try-it-7){#toc-try-it-7}
  - [Closing: The Buffer
    Ecosystem](#closing-the-buffer-ecosystem){#toc-closing-the-buffer-ecosystem}
- [Tutorial: Audio Input, Routing, and Multi-Channel
  Distribution](#tutorial-audio-input-routing-and-multi-channel-distribution){#toc-tutorial-audio-input-routing-and-multi-channel-distribution}
  - [Tutorial: Capturing Audio
    Input](#tutorial-capturing-audio-input){#toc-tutorial-capturing-audio-input}
    - [The Simplest
      Path](#the-simplest-path-3){#toc-the-simplest-path-3}
  - [Expansion 1: What `create_input_listener_buffer()`
    Does](#expansion-1-what-create_input_listener_buffer-does){#toc-expansion-1-what-create_input_listener_buffer-does}
  - [Expansion 2: Manual Input
    Registration](#expansion-2-manual-input-registration){#toc-expansion-2-manual-input-registration}
  - [Expansion 3: Input Without
    Playback](#expansion-3-input-without-playback){#toc-expansion-3-input-without-playback}
  - [Try It](#try-it-8){#toc-try-it-8}
  - [Tutorial: Buffer Supply (Routing to Multiple
    Channels)](#tutorial-buffer-supply-routing-to-multiple-channels){#toc-tutorial-buffer-supply-routing-to-multiple-channels}
    - [The Pattern](#the-pattern-3){#toc-the-pattern-3}
  - [Expansion 1: What "Supply"
    Means](#expansion-1-what-supply-means){#toc-expansion-1-what-supply-means}
  - [Expansion 2: Mix
    Levels](#expansion-2-mix-levels){#toc-expansion-2-mix-levels}
  - [Expansion 3: Removing
    Supply](#expansion-3-removing-supply){#toc-expansion-3-removing-supply}
  - [Try It](#try-it-9){#toc-try-it-9}
  - [Tutorial: Buffer
    Cloning](#tutorial-buffer-cloning){#toc-tutorial-buffer-cloning}
    - [The Pattern](#the-pattern-4){#toc-the-pattern-4}
  - [Expansion 1: Clone
    vs. Supply](#expansion-1-clone-vs.-supply){#toc-expansion-1-clone-vs.-supply}
  - [Expansion 2: Cloning Preserves
    Structure](#expansion-2-cloning-preserves-structure){#toc-expansion-2-cloning-preserves-structure}
  - [Expansion 3: Post-Clone
    Modification](#expansion-3-post-clone-modification){#toc-expansion-3-post-clone-modification}
  - [Try It](#try-it-10){#toc-try-it-10}
  - [Closing: The Routing
    Ecosystem](#closing-the-routing-ecosystem){#toc-closing-the-routing-ecosystem}

---

# Tutorial: Polynomial Waveshaping

## The Simplest Path

Run this code. Your file plays with harmonic distortion.

```cpp
void compose() {
    auto sound = vega.read_audio("path/to/file.wav") | Audio;
    auto buffers = MayaFlux::get_last_created_container_buffers();

    // Polynomial: x² generates harmonics
    auto poly = vega.Polynomial([](double x) { return x * x; });
    auto processor = MayaFlux::create_processor<PolynomialProcessor>(buffers[0], poly);
}
```

Replace `"path/to/file.wav"` with an actual path.

The audio sounds richer, warmer—subtle saturation. That's harmonic content added by the squaring function.

## Expansion 1: Why Polynomials Shape Sound

<details>
<summary>Click to expand: Transfer Functions as Geometry</summary>

When you write `x * x`, you're not "squaring numbers." You're defining a **transfer curve**:

- Input -1.0 → Output 1.0
- Input 0.5 → Output 0.25 (quieter)
- Input 1.0 → Output 1.0 (same)

This asymmetry adds harmonics. The waveform's shape **bends**—its geometry changes.

Analog distortion (tubes, tape) works this way: input voltage doesn't map linearly to output. The circuit's response curve adds character.

Polynomials let you design that curve digitally. `x * x` is gentle. `x * x * x` adds different harmonics (odd instead of even). `std::tanh(x)` mimics tube saturation.

You're sculpting frequency response through function shape.

</details>

## Expansion 2: What `vega.Polynomial()` Creates

<details>
<summary>Click to expand: Nodes vs. Processors</summary>

`vega.Polynomial([](double x) { return x * x; })` creates a **Polynomial node**—a mathematical expression that processes one sample at a time.

By itself, the node doesn't touch your audio. You wrap it in a **PolynomialProcessor**:

```cpp
auto processor = MayaFlux::create_processor<PolynomialProcessor>(buffers[0], poly);
```

**Why this separation?**

- **Node**: The math itself—reusable, chainable, inspectable
- **Processor**: The attachment mechanism—knows _how_ to apply the node to a buffer

Same node, different processors → different results. You'll see this pattern everywhere in MayaFlux.

The node is the _idea_. The processor is the _application_.

</details>

## Expansion 3: PolynomialMode::DIRECT

<details>
<summary>Click to expand: Three Processing Modes</summary>

Polynomials have three modes:

- **DIRECT**: `f(x)` where x is the current sample (what you just used)
- **RECURSIVE**: `f(y[n-1], y[n-2], ...)` where output depends on previous outputs
- **FEEDFORWARD**: `f(x[n], x[n-1], ...)` where output depends on input history

Right now you're using DIRECT mode—each sample transformed independently. This is **memoryless** waveshaping.

Later sections explore time-aware modes. RECURSIVE creates filters and feedback. FEEDFORWARD creates delay-based effects.

For now: DIRECT mode = instant transformation. No memory. No delay.

</details>

## Expansion 4: What `create_processor()` Does

<details>
<summary>Click to expand: Attaching to Buffers</summary>

When you call:

```cpp
auto processor = MayaFlux::create_processor<PolynomialProcessor>(buffers[0], poly);
```

MayaFlux does this:

1. Creates a `PolynomialProcessor` wrapping your polynomial node
2. Gets `buffers[0]`'s processing chain (every buffer has one)
3. Adds the processor to that chain
4. Returns the processor handle

The buffer now runs your polynomial on every cycle:

- 512 samples arrive from the Container
- Your polynomial processes each sample: `y = x * x`
- Transformed samples continue to speakers

The processor is now part of the buffer's flow. It runs automatically every cycle until removed.

</details>

---

## Try It

```cpp
// Cubic distortion (more aggressive, odd harmonics)
auto poly = vega.Polynomial([](double x) { return x * x * x; });

// Chebyshev waveshaping (precise harmonic control)
auto poly = vega.Polynomial([](double x) { return 2*x*x - 1; });

// Soft clipping (analog-style limiting)
auto poly = vega.Polynomial([](double x) {
    return x / (1.0 + std::abs(x));
});

// Extreme fold-back distortion
auto poly = vega.Polynomial([](double x) {
    return std::sin(x * 5.0);
});
```

Listen to each. Same structure, different curves. Each curve generates different harmonic content.

You're not "processing audio"—you're **sculpting the transfer function**.

---

# Tutorial: Recursive Polynomials (Filters and Feedback)

## The Next Step

You have memoryless waveshaping. Now add memory.

```cpp
void compose() {
    auto sound = vega.read_audio("path/to/file.wav") | Audio;
    auto buffers = MayaFlux::get_last_created_container_buffers();

    // Recursive: output depends on previous outputs
    auto recursive = vega.Polynomial(
        [](std::span<double> history) {
            // history[0] = previous output, history[1] = two samples ago
            return 0.5 * history[0] + 0.3 * history[1];
        },
        PolynomialMode::RECURSIVE,
        2  // remember 2 previous outputs
    );

    auto processor = MayaFlux::create_processor<PolynomialProcessor>(buffers[0], recursive);
}
```

Run this. You hear echo/resonance—the signal feeds back into itself.

## Expansion 1: Why This Is a Filter

<details>
<summary>Click to expand: IIR Filters Are Recursive Polynomials</summary>

Classic IIR filter equation:

```
y[n] = b0*x[n] + a1*y[n-1] + a2*y[n-2]
```

Your recursive polynomial **is** that filter—just written as a lambda:

```cpp
[](std::span<double> history) {
    return 0.5 * history[0] + 0.3 * history[1];
}
```

Difference: You can write **nonlinear** feedback:

```cpp
[](std::span<double> history) {
    return history[0] * std::sin(history[1]);  // nonlinear!
}
```

Traditional DSP libraries can't do this. Fixed coefficients only.

Polynomials let you design arbitrary recursive functions—not just linear filters.

</details>

## Expansion 2: The History Buffer

<details>
<summary>Click to expand: How RECURSIVE Mode Works</summary>

When you write:

```cpp
PolynomialMode::RECURSIVE, 2
```

The polynomial maintains a buffer of **previous outputs**:

```
history[0] = y[n-1]  (last output)
history[1] = y[n-2]  (two samples ago)
```

Each cycle:

1. Your lambda reads from `history`
2. Computes new output
3. Polynomial pushes output into `history` (shifts everything down)
4. Loop repeats

The buffer size determines how far back you can look. Larger buffers = longer memory.

For a 100-sample buffer at 48 kHz:

```
100 samples ÷ 48000 Hz ≈ 2 ms of history
```

This is how you build delays, reverbs, resonant filters—anything that needs temporal memory.

</details>

## Expansion 3: Stability Warning

<details>
<summary>Click to expand: Recursive Systems Can Explode</summary>

**Critical rule**: Keep feedback coefficients summing to < 1.0 for guaranteed stability.

**Safe:**

```cpp
return 0.6*history[0] + 0.3*history[1];  // sum = 0.9 < 1.0
```

**Dangerous:**

```cpp
return 1.2*history[0];  // WILL EXPLODE (unbounded growth)
```

Why? Each cycle multiplies previous output by 1.2. Exponential growth. Your speakers won't thank you.

MayaFlux won't stop you—this is a creative tool, not a safety guard. Instability can be interesting (briefly). Controlled feedback explosion creates chaotic textures.

But for stable filters: keep gain < 1.0.

</details>

## Expansion 4: Initial Conditions

<details>
<summary>Click to expand: Seeding the History Buffer</summary>

Recursive polynomials need starting values. Default: `[0.0, 0.0, ...]`

You can seed them:

```cpp
recursive->set_initial_conditions({0.5, -0.3, 0.1});
```

**Why?**

1. **Impulse responses**: Inject energy without external input. The filter "pings" on its own.

2. **Self-oscillation**: Non-zero initial conditions + feedback gain ≥ 1.0 = continuous tone.

3. **Warm start**: Resume from previous state instead of cold-starting at zero.

Example (resonant ping):

```cpp
auto resonator = vega.Polynomial(
    [](std::span<double> history) {
        return 0.99 * history[0] - 0.5 * history[1];
    },
    PolynomialMode::RECURSIVE,
    2
);
resonator->set_initial_conditions({1.0, 0.0});  // kick-start the resonance
```

</details>

---

## Try It

```cpp
// Karplus-Strong string synthesis (plucked string)
auto string = vega.Polynomial(
    [](std::span<double> history) {
        return 0.996 * (history[0] + history[1]) / 2.0;  // lowpass + feedback
    },
    PolynomialMode::RECURSIVE,
    100  // ~480 Hz at 48kHz
);
string->set_initial_conditions(std::vector<double>(100, vega.Random(-1.0, 1.0)));  // noise burst

// Nonlinear resonator (saturating feedback)
auto nonlinear = vega.Polynomial(
    [](std::span<double> history) {
        double fb = 0.8 * history[0];
        return std::tanh(fb * 3.0);  // soft saturation in loop
    },
    PolynomialMode::RECURSIVE,
    1
);

// Comb filter (delay-based coloration)
auto comb = vega.Polynomial(
    [](std::span<double> history) {
        return history[0] + 0.5 * history[50];  // 50-sample delay
    },
    PolynomialMode::RECURSIVE,
    50
);
```

---

# Tutorial: Logic as Decision Maker

## The Simplest Path

Run this code. You'll hear rhythmic pulses.

```cpp
void compose() {
    auto buffer = vega.AudioBuffer()[0] | Audio;

    // Logic node: threshold detection
    auto logic = vega.Logic(LogicOperator::THRESHOLD, 0.0);

    auto processor = MayaFlux::create_processor<LogicProcessor>(
        buffer,
        logic
    );

    processor->set_modulation_type(LogicProcessor::ModulationType::REPLACE);

    // Feed a sine wave into the logic node
    auto sine = vega.Sine(2.0);
    logic->set_input_node(sine);
}
```

What you hear: 2 Hz pulse train—beeps every half second.

The sine wave crosses zero twice per cycle. Logic detects the crossings. Output becomes binary: 1.0 (high) or 0.0 (low).

## Expansion 1: What Logic Does

<details>
<summary>Click to expand: Continuous → Discrete Conversion</summary>

`LogicProcessor` makes **binary decisions** about audio.

Every sample asks: _"Is this value TRUE or FALSE?"_ (based on threshold)

Output: 0.0 or 1.0.

**Uses:**

- **Gate**: Silence audio below threshold (noise reduction)
- **Trigger**: Fire events when signal crosses boundary (drums, envelopes)
- **Rhythm**: Convert continuous modulation into discrete beats

Example: Feed a slow LFO (0.5 Hz sine) into logic → square wave clock.

Digital doesn't care what the input "means"—just whether it passes the test.

</details>

## Expansion 2: Logic node needs an input

<details>
<summary>Click to expand: Continuous → input signal</summary>

`Logic` nodes need an input signal to evaluate.
This is also true for other nodes like `Polynomial`.
So far, you did not have to manually set inputs because you used
`SoundContainerBuffer` which automatically feeds audio into processors.

So, instead of creating an `AudioBuffer`, you can load a file:

```cpp
auto sound = vega.read_audio("path/to/file.wav") | Audio;
auto buffers = MayaFlux::get_last_created_container_buffers();
auto logic = vega.Logic(LogicOperator::THRESHOLD, 0.0);

auto processor = MayaFlux::create_processor<LogicProcessor>(
    buffer[0],
    logic
);

processor->set_modulation_type(LogicProcessor::ModulationType::REPLACE);
```

The audio from the file is automatically fed into the logic node.
Considering how all previous examples relied on file contents, and the natutre of
rhythmic pulses not exploiting the intricacies or richness of audio files,
we are using a sine wave as inputs of the logic node in the main example.

</details>

## Expansion 3: LogicOperator Types

<details>
<summary>Click to expand: Binary Operations</summary>

`LogicOperator` defines the test:

- **THRESHOLD**: `x > threshold` → 1.0, else 0.0
- **HYSTERESIS**: Two thresholds (open/close) to avoid flutter
- **EDGE**: Trigger on transitions (0→1 or 1→0)
- **AND/OR/XOR/NOT**: Boolean algebra on current vs. previous sample
- **CUSTOM**: Your function

Right now you're using THRESHOLD—the simplest test.

Example (hysteresis gate for noisy signals):

```cpp
auto gate = vega.Logic(LogicOperator::HYSTERESIS);
gate->set_hysteresis_thresholds(0.1, 0.3);  // open at 0.3, close at 0.1
```

Signal must exceed 0.3 to open, then drops below 0.1 to close. Prevents rapid on/off flickering.

</details>

## Expansion 4: ModulationType - Readymade Transformations

<details>
<summary>Click to expand: Creative Logic Applications</summary>

`ModulationType` provides readymade ways to apply binary logic to audio:

**Basic Operations:**

- **REPLACE**: Audio becomes 0.0 or 1.0 (bit reduction)
- **MULTIPLY**: Audio × logic (standard gate - preserves timbre)
- **ADD**: Audio + logic (adds impulse on logic high)

**Creative Operations:**

- **INVERT_ON_TRUE**: Phase flip when logic high (ring mod effect)
- **HOLD_ON_FALSE**: Freeze audio when logic low (granular stutter)
- **ZERO_ON_FALSE**: Hard silence when logic low (noise gate)
- **CROSSFADE**: Smooth fade based on logic (dynamic blending)
- **THRESHOLD_REMAP**: Binary amplitude switch (tremolo from logic)
- **SAMPLE_AND_HOLD**: Freeze on logic changes (glitch/stutter)
- **CUSTOM**: Your function

Example (granular freeze effect):

```cpp
processor->set_modulation_type(LogicProcessor::ModulationType::HOLD_ON_FALSE);
// Audio freezes whenever logic goes low - creates stuttering repeats
```

Example (amplitude tremolo):

```cpp
processor->set_modulation_type(LogicProcessor::ModulationType::THRESHOLD_REMAP);
processor->set_threshold_remap_values(1.0, 0.2); // High = full volume, Low = quiet
// Creates rhythmic volume changes based on logic pattern
```

Logic becomes a **compositional control** for transforming audio in musical ways.

</details>

---

## Try It

```cpp
// Hard gate (silence below threshold)
auto gate = vega.Logic(LogicOperator::THRESHOLD, 0.2);
auto proc = MayaFlux::create_processor<LogicProcessor>(buffer, gate);
proc->set_modulation_type(LogicProcessor::ModulationType::ZERO_ON_FALSE);

// Granular stutter (freeze on quiet passages)
auto freeze = vega.Logic(LogicOperator::THRESHOLD, 0.3);
auto proc = MayaFlux::create_processor<LogicProcessor>(buffer, freeze);
proc->set_modulation_type(LogicProcessor::ModulationType::HOLD_ON_FALSE);

// Bit crusher (reduce to 1-bit audio)
auto crusher = vega.Logic(LogicOperator::THRESHOLD, 0.0);
auto proc = MayaFlux::create_processor<LogicProcessor>(buffer, crusher);
proc->set_modulation_type(LogicProcessor::ModulationType::REPLACE);

// Rhythmic tremolo from LFO
auto lfo = vega.Sine(4.0);  // 4 Hz
auto trem_logic = vega.Logic(LogicOperator::THRESHOLD, 0.0);
trem_logic->set_input_node(lfo);
auto proc = MayaFlux::create_processor<LogicProcessor>(buffer, trem_logic);
proc->set_modulation_type(LogicProcessor::ModulationType::THRESHOLD_REMAP);
proc->set_threshold_remap_values(1.0, 0.3);  // Pumping rhythm
```

---

# Tutorial: Combining Polynomial + Logic

## The Pattern

Load a file. Detect transients with logic. Apply polynomial only when transient detected.

```cpp
void compose() {
    auto sound = vega.read_audio("drums.wav") | Audio;
    auto buffers = MayaFlux::get_last_created_container_buffers();

    // Step 1: Detect transients (drum hits)
    auto chain = MayaFlux::create_processing_chain();

    // Step 1: Brutal bitcrushing - reduce to 1-bit
    auto bitcrush = vega.Logic(LogicOperator::THRESHOLD, 0.0);
    auto crush_proc = std::make_shared<LogicProcessor>(bitcrush);
    crush_proc->set_modulation_type(LogicProcessor::ModulationType::REPLACE);

    // Step 2: Freeze audio in chunks - granular stutter
    auto clock = vega.Sine(4.0); // 4 Hz freeze rate
    auto freeze_logic = vega.Logic(LogicOperator::THRESHOLD, 0.0);
    freeze_logic->set_input_node(clock);
    auto freeze_proc = std::make_shared<LogicProcessor>(freeze_logic);
    freeze_proc->set_modulation_type(LogicProcessor::ModulationType::HOLD_ON_FALSE);

    // Step 3: Extreme waveshaping distortion
    auto destroyer = std::make_shared<Polynomial>([](double x) {
        return std::copysign(1.0, x) * std::pow(std::abs(x), 0.3); // Extreme compression
    });
    auto poly_proc = std::make_shared<PolynomialProcessor>(destroyer);

    // Build chain: bitcrush → freeze → destroy
    chain->add_processor(crush_proc, buffers[0]);
    chain->add_processor(freeze_proc, buffers[0]);
    chain->add_processor(poly_proc, buffers[0]);

    buffers[0]->set_processing_chain(chain);
}
```

Or if you want direct control without manual processor creation, you can use the fluent API

```cpp
    auto sound = vega.read_audio("drums.wav") | Audio;
    auto buffers = MayaFlux::get_last_created_container_buffers();

    auto bitcrush = vega.Logic(LogicOperator::THRESHOLD, 0.0);
    auto crush_proc = MayaFlux::create_processor<LogicProcessor>(buffers[0], bitcrush);
    crush_proc->set_modulation_type(LogicProcessor::ModulationType::REPLACE);

    // Step 2: Freeze audio in chunks - granular stutter
    auto clock = vega.Sine(4.0); // 4 Hz freeze rate
    auto freeze_logic = vega.Logic(LogicOperator::THRESHOLD, 0.0);
    freeze_logic->set_input_node(clock);
    auto freeze_proc = MayaFlux::create_processor<LogicProcessor>(buffers[0], freeze_logic);
    freeze_proc->set_modulation_type(LogicProcessor::ModulationType::HOLD_ON_FALSE);

    // Step 3: Extreme waveshaping distortion
    auto destroyer = std::make_shared<Polynomial>([](double x) {
        return std::copysign(1.0, x) * std::pow(std::abs(x), 0.3); // Extreme compression
    });
    auto poly_proc = MayaFlux::create_processor<PolynomialProcessor>(buffers[0], destroyer);

```

## Expansion 1: Processing Chains as Transformation Pipelines

<details>
<summary>Click to expand: Sequential Audio Surgery</summary>

You just built a **transformation pipeline**:

```
bitcrush → freeze → destroy
```

Each processor transforms the output of the previous one. This is **compositional signal processing**—you build complex effects by chaining simple operations.

The power comes from **order dependency**:

```
gate → distort    // Clean transients, heavy saturation
distort → gate    // Distorted everything, then choppy
```

Swap the order = completely different sound.

Extend it:

```
detect transients → sample-and-hold → bitcrush → wavefold → compress
```

Traditional plugins give you "distortion with 3 knobs." You compose the distortion algorithm itself.

**Every processor is a building block.** Chain them to create effects that don't exist as plugins:

- Bitcrush → Freeze → Invert = Glitch stutterer
- Remap → Fold → Gate = Rhythmic harmonizer
- Threshold → Hold → Distort = Transient emphasizer

Logic + Polynomial + Chains = **programmable audio transformation system**.

</details>

## Expansion 2: Chain Order Matters

<details>
<summary>Click to expand: Non-Commutative Processing</summary>

Swap the order of logic and polynomial → different result:

```
Logic → Polynomial  // Detect, then distort
Polynomial → Logic  // Distort, then detect
```

Processors are **non-commutative**. Audio math doesn't follow algebra rules.

Order determines signal flow. You're building a graph, not an equation.

</details>

---

## Try It

```cpp
// Adaptive dynamics (compress quiet, expand loud)
auto logic = vega.Logic(LogicOperator::THRESHOLD, 0.3);
auto poly_compress = vega.Polynomial([](double x) { return x * 2.0; });
auto poly_expand = vega.Polynomial([](double x) { return x * 0.5; });

// Route based on logic state (requires custom modulation)
```

---

# Tutorial: Processing Chains and Buffer Architecture

## Tutorial: Explicit Chain Building

### The Simplest Path

You've been adding processors one at a time. Now control their order explicitly.

```cpp
void compose() {
    auto sound = vega.read_audio("path/to/file.wav") | Audio;
    auto buffer = MayaFlux::get_last_created_container_buffers()[0];

    // Create an empty chain
    auto chain = MayaFlux::create_processing_chain();

    // Build the chain: Distortion → Gate → Compression
    auto distortion = vega.Polynomial([](double x) { return std::tanh(x * 2.0); });
    auto gate = vega.Logic(LogicOperator::THRESHOLD, 0.1);
    auto compression = vega.Polynomial([](double x) { return x / (1.0 + std::abs(x)); });

    chain->add_processor(std::make_shared<PolynomialProcessor>(distortion), buffer);
    chain->add_processor(std::make_shared<LogicProcessor>(gate), buffer);
    chain->add_processor(std::make_shared<PolynomialProcessor>(compression), buffer);

    // Attach the chain to the buffer
    buffer->set_processing_chain(chain);
}
```

Run this. You hear: clean audio → saturated → gated (silence below threshold) → compressed (controlled peaks).

**Swap the order:**

```cpp
chain->add_processor(gate_processor);      // Gate first
chain->add_processor(distortion_processor); // Then distort
chain->add_processor(compression_processor);
```

Different sound. Order matters.

## Expansion 1: What `create_processor()` Was Doing

<details>
<summary>Click to expand: Implicit vs. Explicit Chain Management</summary>

Previously, when you wrote:

```cpp
auto processor = MayaFlux::create_processor<PolynomialProcessor>(buffer, poly);
```

MayaFlux did this behind the scenes:

1. Created the processor
2. Got the buffer's existing processing chain
3. **Automatically added the processor to that chain**
4. Returned the processor

You didn't see this because it was implicit. The processor was silently appended to whatever chain existed.

**Now you're building chains explicitly:**

```cpp
auto chain = MayaFlux::create_processing_chain();  // Empty chain
chain->add_processor(proc1);  // Manual control
chain->add_processor(proc2);
buffer->set_processing_chain(chain);  // Replace buffer's chain
```

**When to use explicit chains:**

- You need precise order control
- You're building reusable processor "presets"
- You want to swap entire chains dynamically (e.g., switch between clean/distorted modes)
- You're debugging processor interactions

**When implicit is fine:**

- Simple cases (1-2 processors)
- Order doesn't matter (parallel-like effects)
- Rapid prototyping

</details>

## Expansion 2: Chain Execution Order

<details>
<summary>Click to expand: Sequential Data Flow</summary>

Chains execute like a for-loop over processors:

```cpp
for (auto& processor : chain->get_processors()) {
    processor->process(buffer);
}
```

Data flows sequentially:

```
Container → Buffer (512 samples)
    ↓
Processor₁: Distortion (modifies samples in-place)
    ↓
Processor₂: Gate (zeroes out quiet samples)
    ↓
Processor₃: Compression (reduces peaks)
    ↓
Speakers
```

Each processor sees the **output** of the previous processor.

**This is not parallel processing.** No branches. No simultaneous paths. Pure sequential transformation.

(Parallel routing requires `BufferPipeline`—covered in a later tutorial.)

</details>

## Expansion 3: Default Processors vs. Chain Processors

<details>
<summary>Click to expand: The Two-Stage Processing Model</summary>

Every buffer has two processing stages:

**Stage 1: Default Processor** (runs first, always)

- Defined by buffer type
- Handles data **acquisition** or **generation**
- Examples:
  - `SoundContainerBuffer`: reads from file/stream
  - `NodeBuffer`: evaluates a node
  - `FeedbackBuffer`: mixes current + previous buffer
  - `AudioBuffer`: none (generic accumulator)

**Stage 2: Processing Chain** (runs second)

- Your custom processors
- Handles data **transformation**
- Examples: filters, waveshaping, logic, etc.

**Execution flow:**

```
1. Buffer's default processor runs (fills buffer with data)
2. Processing chain runs (transforms that data)
3. Result goes to speakers
```

When you add processors via `create_processor()`, they go into **Stage 2** (the chain).

The **default processor** is fixed per buffer type. You can replace it, but usually you don't need to—the chain is where creativity happens.

</details>

---

## Try It

```cpp
// Stack multiple distortions (cascading saturation)
auto chain = MayaFlux::create_processing_chain();
auto light = vega.Polynomial([](double x) { return std::tanh(x * 1.5); });
auto heavy = vega.Polynomial([](double x) { return std::tanh(x * 5.0); });
auto fold = vega.Polynomial([](double x) { return std::sin(x * 3.0); });

chain->add_processor(std::make_shared<PolynomialProcessor>( light), buffer);
chain->add_processor(std::make_shared<PolynomialProcessor>( heavy), buffer);
chain->add_processor(std::make_shared<PolynomialProcessor>( fold), buffer);

// Insert gating between stages
auto gate = vega.Logic(LogicOperator::THRESHOLD, 0.2);
chain->add_processor(std::make_shared<PolynomialProcessor>( light), buffer);
chain->add_processor(std::make_shared<LogicProcessor>( gate), buffer);  // Gate the distortion
chain->add_processor(std::make_shared<PolynomialProcessor>( heavy), buffer); // Distort the gated signal

buffer->set_processing_chain(chain);
```

---

# Tutorial: Various Buffer Types

## Generating from Nodes (NodeBuffer)

### The Next Pattern

So far: buffers read from files, nodes affect buffer processing.
Now: buffers **generate** from nodes.

```cpp
void compose() {
    // Create a sine node
    auto sine = vega.Sine(440.0);

    // Create a NodeBuffer that captures the sine's output
    auto node_buffer = vega.NodeBuffer(0, 512, sine)[0] | Audio;

    // Add processing to the generated audio
    auto distortion = vega.Polynomial([](double x) { return x * x * x; });
    MayaFlux::create_processor<PolynomialProcessor>(node_buffer, distortion);
}
```

Run this. You hear a 440 Hz sine wave with cubic distortion.

No file loaded. The buffer **generates** audio by evaluating the node 512 times per cycle.

### Expansion 1: What NodeBuffer Does

<details>
<summary>Click to expand: Nodes → Buffers Bridge</summary>

`NodeBuffer` connects the **node system** (sample-by-sample evaluation) to the **buffer system** (block-based processing).

**Default processor: `NodeSourceProcessor`**

Each cycle:

1. Node is evaluated 512 times: `node->process_sample()`
2. Results fill the buffer
3. Processing chain runs (your custom processors)
4. Buffer outputs to speakers

**Why this matters:**

Nodes are mathematical expressions—infinite generators. Buffers are temporal accumulators—finite chunks.

`NodeBuffer` bridges the two: **continuous expression → discrete blocks**.

Without `NodeBuffer`, you'd manually call `node->process_sample()` 512 times and copy results into a buffer. `NodeBuffer` automates this.

</details>

### Expansion 2: The `clear_before_process` Parameter

<details>
<summary>Click to expand: Accumulation vs. Replacement</summary>

`NodeBuffer` has a flag: `clear_before_process`

```cpp
auto node_buffer = vega.NodeBuffer(0, 512, sine, true);  // Clear first (default)
```

**true (default)**: Buffer is zeroed, then filled with node output

- Result: pure node output

**false**: Node output is **added** to existing buffer content

- Result: node output + previous buffer state

Why use `false`?

- **Layering**: Multiple nodes contributing to the same buffer
- **Feedback**: Previous cycle's output influences current cycle
- **Additive synthesis**: Mix multiple generators

Example (layering):

```cpp
auto sine = vega.Sine(440.0);
auto buffer = vega.NodeBuffer(0, 512, sine, true)[0] | Audio;  // First node clears

auto noise = vega.Random();
auto noise_buffer = vega.NodeBuffer(0, 512, noise, false)[0] | Audio;  // Adds to sine
```

Result: sine + noise.

</details>

### Expansion 3: NodeSourceProcessor Mix Parameter

<details>
<summary>Click to expand: Interpolation Between Existing and Incoming Data</summary>

`NodeSourceProcessor` has a `mix` parameter (default: 0.5):

```cpp
auto processor = std::make_shared<NodeSourceProcessor>(node, 0.7f);
```

**Mix = 0.0**: Preserve existing buffer content (node output ignored)
**Mix = 0.5**: Equal blend of existing + node output
**Mix = 1.0**: Replace with node output (existing content overwritten)

This is a **cross-fade** between what's in the buffer and what the node generates.

**Use case**: Smoothly transition between sources, or create feedback loops where node output gradually replaces decaying buffer content.

Most of the time, you'll use the default (1.0 via `clear_before_process=true`). But for creative effects, `mix` is powerful.

</details>

---

### Try It

```cpp
// Additive synthesis (multiple generators in one buffer)
auto fund = vega.Sine(220.0);
auto harm2 = vega.Sine(440.0);
auto harm3 = vega.Sine(660.0);

auto buffer = vega.NodeBuffer(0, 512, fund, true)[0] | Audio;   // First clears
vega.NodeBuffer(0, 512, harm2, false)[0] | Audio;  // Adds harmonic 2
vega.NodeBuffer(0, 512, harm3, false)[0] | Audio;  // Adds harmonic 3

// Waveshaping a generated tone
auto sine = vega.Sine(110.0);
auto buffer2 = vega.NodeBuffer(0, 512, sine)[1] | Audio;
auto waveshape = vega.Polynomial([](double x) { return std::tanh(x * 10.0); });
MayaFlux::create_processor<PolynomialProcessor>(buffer2, waveshape);
```

---

## FeedbackBuffer (Recursive Audio)

### The Pattern

Buffers that **remember** their previous state.

```cpp
void compose() {
    // FeedbackBuffer: 70% feedback, 512 samples delay
    auto feedback_buf = vega.FeedbackBuffer(0, 512, 0.7f, 512)[0] | Audio;

    // Feed an impulse into the buffer to kick-start resonance
    auto impulse = vega.Impulse(2.0);  // 2 Hz pulse train
    vega.NodeBuffer(0, 512, impulse, false)[0] | Audio;  // Adds to feedback buffer

    // WARN: Remember to turn OFF aftera a few seconds as feedback can build up!
}
```

Run this. You hear: repeating echoes, each 70% of the previous amplitude.

The buffer **feeds back into itself**—output becomes input next cycle.

### Expansion 1: What FeedbackBuffer Does

<details>
<summary>Click to expand: Recursive Temporal Processing</summary>

**Default processor: `FeedbackProcessor`**

Each cycle:

1. Current buffer content: `buffer[n]`
2. Previous buffer content: `previous_buffer[n-1]`
3. Output: `buffer[n] + (feedback_amount * previous_buffer[n-1])`
4. Store output as next cycle's "previous"

This is a **simple delay line** with feedback.

**Parameters:**

- `feedback_amount`: 0.0–1.0 (how much previous state contributes)
- `feed_samples`: Delay length in samples

Example: `FeedbackBuffer(0, 512, 0.7, 512)` creates:

- 512-sample delay (~10.6 ms at 48 kHz)
- 70% feedback (echoes decay to 0.7 → 0.49 → 0.343 → ...)

**Stability:** Keep `feedback_amount < 1.0` or output will grow unbounded.

</details>

### Expansion 2: FeedbackBuffer Limitations

<details>
<summary>Click to expand: What FeedbackBuffer Cannot Do</summary>

`FeedbackBuffer` is simple—intentionally. It implements **one specific recursive algorithm**: linear feedback delay.

**Limitations:**

1. **Fixed feedback coefficient**: Can't modulate feedback amount per sample (it's buffer-wide)
2. **No filtering in loop**: Can't insert lowpass/highpass in the feedback path
3. **No cross-channel feedback**: Single-channel only
4. **No time-varying delay**: Delay length is fixed at creation

**Why these limitations?**

`FeedbackBuffer` is a **building block**, not a complete reverb/delay effect.

For complex feedback systems:

- Use `PolynomialProcessor` in `RECURSIVE` mode (per-sample nonlinear feedback)
- Use `BufferPipeline` to route buffers back to themselves with processing
- Build custom feedback networks with multiple buffers

`FeedbackBuffer` is for **simple echoes and resonances**—quick and efficient.

</details>

### Expansion 3: When to Use FeedbackBuffer

<details>
<summary>Click to expand: Use Cases and Alternatives</summary>

**Use `FeedbackBuffer` when:**

- You need a simple delay line with fixed feedback
- Building Karplus-Strong string synthesis
- Creating rhythmic echoes
- Implementing comb filters

**Use `PolynomialProcessor(RECURSIVE)` when:**

- You need nonlinear feedback (saturation, distortion in loop)
- Feedback amount varies per sample
- Building filters with arbitrary feedback functions

**Use `BufferPipeline` when:**

- You need complex routing (buffer A → process → buffer B → back to A)
- Multi-buffer feedback networks
- Cross-channel feedback

**Example: Filtered feedback (requires multiple approaches):**

```cpp
// FeedbackBuffer can't do this alone:
// current + lowpass(feedback * previous)

// Solution: Use PolynomialProcessor RECURSIVE mode with filtering
auto filtered_feedback = vega.Polynomial(
    [](std::span<double> history) {
        double fb = 0.7 * history[0];
        return fb * 0.5 + history[1] * 0.5;  // Simple lowpass
    },
    PolynomialMode::RECURSIVE,
    2
);
```

</details>

---

### Try It

```cpp
// Karplus-Strong string (plucked string synthesis)
auto feedback_buf = vega.FeedbackBuffer(0, 512, 0.996f, 100)[0] | Audio;  // ~480 Hz
// Excite with noise burst
auto noise = vega.Random();
vega.NodeBuffer(0, 512, noise, false)[0] | Audio;

// Ping-pong delay (requires two buffers—teaser for later)
// auto left = vega.FeedbackBuffer(0, 512, 0.6f, 2400)[0] | Audio;
// auto right = vega.FeedbackBuffer(1, 512, 0.6f, 2400)[1] | Audio;
// Route left → right, right → left (needs BufferPipeline)

// Resonant comb filter
auto feedback_buf2 = vega.FeedbackBuffer(0, 512, 0.95f, 50)[1] | Audio;
auto input = vega.Sine(220.0);
vega.NodeBuffer(0, 512, input, false)[1] | Audio;
```

---

## SoundStreamWriter (Capturing Audio)

### The Pattern

Processors that **write** buffer data somewhere (instead of transforming it).

```cpp
void compose() {
    auto sound = vega.read_audio("path/to/file.wav") | Audio;
    auto buffer = MayaFlux::get_last_created_container_buffers()[0];

    // Create a DynamicSoundStream (accumulator for captured audio)
    auto capture_stream = std::make_shared<DynamicSoundStream>(48000, 2);

    // Create a processor that writes buffer data to the stream
    auto writer = std::make_shared<SoundStreamWriter>(capture_stream);

    // Add to buffer's processing chain
    auto chain = buffer->get_processing_chain();
    chain->add_processor(writer);

    // File plays AND is captured to stream simultaneously
}
```

Run this. The file plays **and** is written to `capture_stream` every cycle.

After playback, `capture_stream` contains a copy of the entire file (processed through any other processors in the chain before the writer).

### Expansion 1: What SoundStreamWriter Does

<details>
<summary>Click to expand: Buffers → Containers Bridge</summary>

`SoundStreamWriter` is the **inverse** of `SoundContainerBuffer`:

- **SoundContainerBuffer**: reads from container → fills buffer (source)
- **SoundStreamWriter**: reads from buffer → writes to container (sink)

**Each cycle:**

1. Extract 512 samples from the buffer
2. Write them to the `DynamicSoundStream` at the current write position
3. Increment write position by 512

The stream grows dynamically as data arrives. No pre-allocation needed (though you can for performance).

**Use cases:**

- Record processed audio to memory
- Capture intermediate processing stages for analysis
- Build delay lines / loopers
- Create feedback paths (buffer → stream → buffer)

</details>

### Expansion 2: Channel-Aware Writing

<details>
<summary>Click to expand: Multi-Channel Capture</summary>

`SoundStreamWriter` respects buffer channel IDs:

```cpp
auto left_buffer = buffers[0];   // channel 0
auto right_buffer = buffers[1];  // channel 1

auto stream = std::make_shared<DynamicSoundStream>(48000, 2);  // stereo

auto writer_L = std::make_shared<SoundStreamWriter>(stream);
auto writer_R = std::make_shared<SoundStreamWriter>(stream);

// Add to respective buffers
left_buffer->get_processing_chain()->add_processor(writer_L);
right_buffer->get_processing_chain()->add_processor(writer_R);
```

Result: Stereo file captured to stereo stream—channels preserved.

**Critical:** Buffer's `channel_id` determines which stream channel receives data. Mismatch = warning + skip.

</details>

### Expansion 3: Position Management

<details>
<summary>Click to expand: Write Position Control</summary>

`SoundStreamWriter` tracks where it's writing:

```cpp
writer->set_write_position(0);        // Write from start
writer->set_write_position(48000);    // Write from 1-second mark
writer->reset_position();             // Reset to 0

// Time-based positioning
writer->set_write_position_time(2.5); // Write from 2.5 seconds

uint64_t pos = writer->get_write_position();           // Get current frame position
double time = writer->get_write_position_time();       // Get current time position
```

**Why control position?**

- **Overdubbing**: Write new audio over existing content
- **Looping**: Reset position to create cyclic recording
- **Multi-pass recording**: Capture different takes at different positions

Default behavior: append at end. Position auto-increments.

</details>

### Expansion 4: Circular Mode

<details>
<summary>Click to expand: Fixed-Size Circular Buffers</summary>

`DynamicSoundStream` can operate in **circular mode**:

```cpp
auto stream = std::make_shared<DynamicSoundStream>(48000, 2);
stream->enable_circular_buffer(48000);  // 1 second capacity

auto writer = std::make_shared<SoundStreamWriter>(stream);
```

**Behavior:**

When write position reaches capacity, it wraps to 0. Old data is overwritten.

**Use cases:**

- **Delay lines**: Fixed-length delays for effects
- **Loopers**: Record N seconds, then loop
- **Rolling analysis**: Keep only the most recent N seconds

Without circular mode, the stream grows unbounded—useful for full recording, problematic for long-running systems.

</details>

---

### Try It

```cpp
// Record 5 seconds of audio
auto sound = vega.read_audio("path/to/file.wav") | Audio;
auto buffer = MayaFlux::get_last_created_container_buffers()[0];

auto stream = std::make_shared<DynamicSoundStream>(48000, 1);
stream->ensure_capacity(48000 * 5);  // Pre-allocate 5 seconds

auto writer = std::make_shared<SoundStreamWriter>(stream);
buffer->get_processing_chain()->add_processor(writer, buffer);

// After playback, stream contains the audio
// You can analyze it, write to disk, or feed it back

// Circular delay (1 second)
auto stream2 = std::make_shared<DynamicSoundStream>(48000, 1);
stream2->enable_circular_buffer(48000);  // Loop after 1 second

auto writer2 = std::make_shared<SoundStreamWriter>(stream);
buffer->get_processing_chain()->add_processor(writer, buffer);

// Stream now acts as a 1-second tape loop
```

## Closing: The Buffer Ecosystem

You now understand:

**Buffer Types:**

- `AudioBuffer`: Generic accumulator
- `SoundContainerBuffer`: Reads from files/streams (default: `SoundStreamReader`)
- `NodeBuffer`: Generates from nodes (default: `NodeSourceProcessor`)
- `FeedbackBuffer`: Recursive delay (default: `FeedbackProcessor`)

**Processor Types:**

- `PolynomialProcessor`: Waveshaping, filters, recursive math
- `LogicProcessor`: Decisions, gates, triggers
- `SoundStreamWriter`: Capture to containers

**Processing Flow:**

```
Default Processor (acquire/generate data)
    ↓
Processing Chain (transform data)
    ↓
Output (speakers/containers/other buffers)
```

**Next:** Buffer routing, cloning, and supply mechanics—how to send processed buffers to multiple channels/domains.

---

# Tutorial: Audio Input, Routing, and Multi-Channel Distribution

## Tutorial: Capturing Audio Input

### The Simplest Path

So far: buffers read from files or generate from nodes. Now: capture from your microphone.

```cpp
void settings() {
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.input.enabled = true;   // Enable microphone input
    stream.input.channels = 1;      // Mono input
}

void compose() {
    // Create a buffer that listens to microphone channel 0
    auto mic_buffer = MayaFlux::create_input_listener_buffer(0, true);

    // Add processing to the live input
    auto distortion = vega.Polynomial([](double x) { return std::tanh(x * 3.0); });
    MayaFlux::create_processor<PolynomialProcessor>(mic_buffer, distortion);
}
```

Run this. Speak into your microphone. You hear yourself with distortion applied in real-time.

## Expansion 1: What `create_input_listener_buffer()` Does

<details>
<summary>Click to expand: Input System Architecture</summary>

MayaFlux has a dedicated **input subsystem** parallel to the output system.

**Architecture:**

```
Hardware (Microphone)
    ↓
Audio Driver (RtAudio)
    ↓
BufferManager::process_input()
    ↓
InputAudioBuffer (per input channel)
    ↓
InputAccessProcessor (dispatches to listeners)
    ↓
Your listener buffers
```

When you call `create_input_listener_buffer(channel, add_to_output)`:

1. Creates a new `AudioBuffer`
2. Registers it with `InputAudioBuffer[channel]` as a **listener**
3. If `add_to_output=true`: Also registers it with output channel (so it plays back)

**Each audio cycle:**

- Driver captures microphone data
- `InputAudioBuffer` receives it
- `InputAccessProcessor` **copies** data to all registered listeners
- Your buffer gets fresh input every cycle

**Key insight:** `InputAudioBuffer` is a **hub**. Multiple buffers can listen to the same input channel simultaneously.

</details>

## Expansion 2: Manual Input Registration

<details>
<summary>Click to expand: Fine-Grained Control</summary>

`create_input_listener_buffer()` is convenience. You can do it manually:

```cpp
// Create your own buffer
auto buffer = vega.AudioBuffer()[0] | Audio;

// Register it as input listener
MayaFlux::read_from_audio_input(buffer, 0);  // Listen to input channel 0

// Later, stop listening:
MayaFlux::detach_from_audio_input(buffer, 0);
```

**When to use manual registration:**

- You already have a buffer (don't want to create a new one)
- You want to dynamically start/stop listening (e.g., record button)
- You need finer control over buffer lifecycle

**Example: Record button**

```cpp
auto recorder = vega.AudioBuffer()[0] | Audio;

// Start recording
MayaFlux::read_from_audio_input(recorder, 0);

// Stop recording (after some time)
MayaFlux::detach_from_audio_input(recorder, 0);
```

The buffer continues to exist and process, but stops receiving new input.

</details>

## Expansion 3: Input Without Playback

<details>
<summary>Click to expand: Silent Capture</summary>

Often you want to **capture** input without **playing** it back:

```cpp
// Create listener but don't add to output
auto mic_capture = MayaFlux::create_input_listener_buffer(0, false);  // false = silent

// Capture to a stream for analysis
auto stream = std::make_shared<DynamicSoundStream>(48000, 1);
auto writer = std::make_shared<SoundStreamWriter>(stream);
mic_capture->get_processing_chain()->add_processor(writer);
```

**Result:** Microphone data is captured to `stream`, but you don't hear it.

**Use cases:**

- Recording without monitoring
- Voice analysis (pitch detection, speech recognition)
- Trigger detection (clap to start/stop)
- Level metering / VU display

</details>

---

## Try It

```cpp
// Real-time vocal effects chain
auto mic = MayaFlux::create_input_listener_buffer(0, true);

auto pitch_shift = vega.Polynomial([](double x) { return x * 1.5; });  // Naive pitch shift
auto reverb = vega.FeedbackBuffer(0, 512, 0.3f, 2400);  // Simple reverb
auto gate = vega.Logic(LogicOperator::THRESHOLD, 0.05);  // Noise gate

MayaFlux::create_processor<PolynomialProcessor>(mic, pitch_shift);
MayaFlux::create_processor<LogicProcessor>(mic, gate);

// Record to disk simultaneously
auto stream = std::make_shared<DynamicSoundStream>(48000, 1);
auto writer = std::make_shared<SoundStreamWriter>(stream);
mic->get_processing_chain()->add_processor(writer, mic);
// After session: save 'stream' to file

// Voice-triggered synthesis
auto mic_silent = MayaFlux::create_input_listener_buffer(0, false);
auto trigger = vega.Logic(LogicOperator::EDGE);
trigger->set_edge_detection(EdgeType::RISING, 0.3);
auto trigger_proc = MayaFlux::create_processor<LogicProcessor>(mic_silent, trigger);

// When trigger fires, start a synthesizer...
```

---

## Tutorial: Buffer Supply (Routing to Multiple Channels)

### The Pattern

One buffer, multiple output channels.

```cpp
void compose() {
    auto sine = vega.Sine(440.0);
    auto buffer = vega.NodeBuffer(0, 512, sine)[0] | Audio;  // Registered to channel 0

    // Supply this buffer to channels 1 and 2 as well
    MayaFlux::supply_buffer_to_channels(buffer, {1, 2}, 0.5);  // 50% mix level
}
```

Run this. You hear the same 440 Hz sine on **all three channels** (left, center, right in surround setup).

The buffer processes **once**, but outputs to **three channels**.

## Expansion 1: What "Supply" Means

<details>
<summary>Click to expand: The Difference Between Registration and Supply</summary>

**Registration** (`vega.AudioBuffer()[0] | Audio`):

- Adds buffer as a **child** of `RootAudioBuffer[0]`
- Buffer processes during channel 0's cycle
- Output **accumulates** into channel 0

**Supply** (`supply_buffer_to_channels`):

- Adds buffer's **output** to other channels
- Buffer still processes in its original channel
- Output is **copied** to supplied channels

**Analogy:**

- Registration = "This buffer lives in channel 0"
- Supply = "After processing in channel 0, send copies to channels 1 and 2"

**Architecture:**

```
Buffer processes in channel 0
    ↓
Output goes to RootAudioBuffer[0]
    ↓
MixProcessor copies output to RootAudioBuffer[1]
    ↓
MixProcessor copies output to RootAudioBuffer[2]
```

**Key:** The buffer only processes **once**. Supply is a **routing** operation, not a duplication of processing.

</details>

## Expansion 2: Mix Levels

<details>
<summary>Click to expand: Controlling Supply Amplitude</summary>

The `mix` parameter controls how much of the buffer's output is sent:

```cpp
MayaFlux::supply_buffer_to_channel(buffer, 1, 1.0);  // 100% (unity gain)
MayaFlux::supply_buffer_to_channel(buffer, 2, 0.5);  // 50% (half amplitude)
MayaFlux::supply_buffer_to_channel(buffer, 3, 0.1);  // 10% (quiet)
```

**Use case: Stereo width control**

```cpp
auto mono_source = vega.Sine(440.0);
auto buffer = vega.NodeBuffer(0, 512, mono_source)[0] | Audio;

// Send to left (full) and right (half) for asymmetric stereo
MayaFlux::supply_buffer_to_channel(buffer, 0, 1.0);  // Left
MayaFlux::supply_buffer_to_channel(buffer, 1, 0.5);  // Right (quieter)
```

**Use case: Send effects**

```cpp
auto dry = vega.NodeBuffer(0, 512, sine)[0] | Audio;  // Dry signal, channel 0

// Send 30% to reverb channel
MayaFlux::supply_buffer_to_channel(dry, 2, 0.3);  // Channel 2 = reverb bus
```

Mix is **additive**. If channel already has content, supply **adds** to it.

</details>

## Expansion 3: Removing Supply

<details>
<summary>Click to expand: Dynamic Routing Changes</summary>

You can remove supply relationships:

```cpp
auto buffer = vega.NodeBuffer(0, 512, sine)[0] | Audio;
MayaFlux::supply_buffer_to_channel(buffer, 1);

// Later: stop sending to channel 1
MayaFlux::remove_supplied_buffer_from_channel(buffer, 1);

// Or remove from multiple channels at once
MayaFlux::remove_supplied_buffer_from_channels(buffer, {1, 2, 3});
```

**Use case: Mute individual sends**

- Buffer still processes
- Output still goes to its registered channel
- Supplied channels no longer receive it

**Use case: Dynamic routing matrices**

```cpp
if (user_pressed_button_A) {
    MayaFlux::supply_buffer_to_channel(buffer, 1);
} else {
    MayaFlux::remove_supplied_buffer_from_channel(buffer, 1);
}
```

</details>

## Try It

```cpp
// Quad panning (4-channel surround)
auto source = vega.Sine(220.0);
auto buffer = vega.NodeBuffer(0, 512, source)[0] | Audio;

// Distribute to 4 corners with different levels (panning)
MayaFlux::supply_buffer_to_channel(buffer, 0, 0.7);  // Front-left
MayaFlux::supply_buffer_to_channel(buffer, 1, 0.3);  // Front-right
MayaFlux::supply_buffer_to_channel(buffer, 2, 0.2);  // Rear-left
MayaFlux::supply_buffer_to_channel(buffer, 3, 0.1);  // Rear-right

// Send effects architecture
auto guitar = vega.NodeBuffer(0, 512, source)[0] | Audio;  // Channel 0 = dry
auto reverb_bus = vega.FeedbackBuffer(1, 512, 0.7f, 4800)[1] | Audio;  // Channel 1 = reverb
auto delay_bus = vega.FeedbackBuffer(2, 512, 0.6f, 9600)[2] | Audio;   // Channel 2 = delay

MayaFlux::supply_buffer_to_channel(guitar, 1, 0.4);  // 40% to reverb
MayaFlux::supply_buffer_to_channel(guitar, 2, 0.2);  // 20% to delay

// Multi-band processing (split frequency ranges across channels)
// Process each band independently, then sum
```

---

## Tutorial: Buffer Cloning

### The Pattern

One buffer specification, multiple independent instances.

```cpp
void compose() {
    auto sine = vega.Sine(440.0);
    auto buffer = vega.NodeBuffer(0, 512, sine);  // Don't register yet

    // Clone to channels 0, 1, 2
    MayaFlux::clone_buffer_to_channels(buffer, {0, 1, 2});
}
```

Run this. You hear **three independent sine waves** on three channels.

Each clone processes **independently**—they don't share data.

## Expansion 1: Clone vs. Supply

<details>
<summary>Click to expand: When to Use Each</summary>

**Supply:**

- One buffer processes **once**
- Output is **copied** to multiple channels
- Processing cost: **1× processing**
- Memory: **One buffer**
- Use when: Same signal needs to go to multiple places

**Clone:**

- Multiple buffers process **independently**
- Each has its own data, state, processing chain
- Processing cost: **N× processing** (N = number of clones)
- Memory: **N buffers**
- Use when: Similar buffers need independent processing

**Example: Supply use case**

```cpp
// One reverb output to stereo speakers
auto reverb = vega.FeedbackBuffer(0, 512, 0.8f, 4800)[0] | Audio;
MayaFlux::supply_buffer_to_channel(reverb, 1);  // Copy to right channel
// Cost: 1× reverb processing
```

**Example: Clone use case**

```cpp
// Independent noise generators per channel
auto noise_template = vega.NodeBuffer(0, 512, vega.Random(-1.0, 1.0));
MayaFlux::clone_buffer_to_channels(noise_template, {0, 1, 2, 3});
// Cost: 4× noise processing (each with different random seed/state)
// Result: Decorrelated noise on each channel
```

</details>

## Expansion 2: Cloning Preserves Structure

<details>
<summary>Click to expand: What Gets Cloned</summary>

When you clone a buffer, each clone receives:

- **Same buffer type** (NodeBuffer, FeedbackBuffer, etc.)
- **Same default processor** configuration
- **Same processing chain** (all added processors)
- **Independent data** (not shared—each clone has its own samples)
- **Independent state** (feedback buffers have separate history)

**Example: Clone a processed buffer**

```cpp
auto sine = vega.Sine(440.0);
auto buffer = vega.NodeBuffer(0, 512, sine);

// Add processing before cloning
auto distortion = vega.Polynomial([](double x) { return std::tanh(x * 2.0); });
MayaFlux::create_processor<PolynomialProcessor>(buffer, distortion);

// Now clone
MayaFlux::clone_buffer_to_channels(buffer, {0, 1, 2});

// Result: Each channel gets sine → distortion (independently processed)
```

Each clone has its own instance of the distortion processor. They don't share state.

</details>

## Expansion 3: Post-Clone Modification

<details>
<summary>Click to expand: Differentiating Clones After Creation</summary>

After cloning, you can modify individual clones:

```cpp
auto buffer = vega.NodeBuffer(0, 512, vega.Sine(440.0));
// Store cloned buffers for later reference
auto cloned_buffers = MayaFlux::clone_buffer_to_channels(buffer, { 0, 1, 2 });


// Add different processing to each
std::vector<double> coeffs_a_2 = { 0.2, 0.3, 0.2 };
std::vector<double> coeffs_b_2 = { 1.0, -0.7 };
auto filter1 = vega.IIR(coeffs_a_1, coeffs_b_1);
auto filter2 = vega.IIR(coeffs_a_2, coeffs_b_2);

MayaFlux::create_processor<FilterProcessor>(cloned_buffers[0], filter1);
MayaFlux::create_processor<FilterProcessor>(cloned_buffers[1], filter2);

// Now channel 0 has one filter, channel 1 has a different filter
```

**Use case:** Stereo decorrelation (same source, slightly different processing per channel)

</details>

---

## Try It

```cpp
// Stereo chorus (cloned with phase offset)
auto lfo = vega.Sine(0.5);  // Slow LFO
auto buffer = vega.NodeBuffer(0, 512, lfo);
MayaFlux::clone_buffer_to_channels(buffer, {0, 1});
// Modify one clone to have phase offset (requires accessing clone directly)

// Multi-channel granular synthesis
auto grain_template = vega.NodeBuffer(0, 512, vega.Random(-0.1, 0.1));
MayaFlux::clone_buffer_to_channels(grain_template, {0, 1, 2, 3, 4, 5, 6, 7});
// Each channel generates independent grains

// Independent feedback loops per channel
auto feedback_template = vega.FeedbackBuffer(0, 512, 0.8f, 1000);
MayaFlux::clone_buffer_to_channels(feedback_template, {0, 1, 2, 3});
// Excite each with different input → 4 independent resonances
```

---

## Closing: The Routing Ecosystem

You now understand:

**Input Capture:**

- `InputAudioBuffer`: Hardware input hub
- `InputAccessProcessor`: Dispatches to listeners
- `create_input_listener_buffer()`: Quick setup
- `read_from_audio_input()` / `detach_from_audio_input()`: Manual control

**Buffer Supply:**

- `supply_buffer_to_channel()`: Route one buffer to multiple outputs
- Mix levels: Control send amounts
- Efficiency: Process once, output many times
- `remove_supplied_buffer_from_channel()`: Dynamic routing changes

**Buffer Cloning:**

- `clone_buffer_to_channels()`: Create independent copies
- Preserves structure: Type, processors, chains
- Independent state: Each clone processes separately
- Post-clone modification: Differentiate behavior after creation

**Mental Model:**

```
Input (Microphone)
    ↓
InputAudioBuffer → Listener buffers (capture)
    ↓
Processing chains (transform)
    ↓
Supply (route to multiple channels)
    OR
Clone (create independent instances)
    ↓
RootAudioBuffer (mix per channel)
    ↓
Output (Speakers)
```

**Next:** BufferPipeline (declarative multi-stage workflows with temporal control)

---
