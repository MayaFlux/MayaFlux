# Contributing: Logging System Migration

## Overview

We're migrating from raw `std::cerr`, `std::cout`, and `throw` statements to our unified `Journal` logging system. This is a **great first contribution** that helps you learn the codebase while making a real impact!

## 🎯 Why This Matters

- Consistent error reporting across the entire engine
- Proper source location tracking for debugging
- Component-based filtering for development
- Real-time safe logging for audio callbacks
- Better production diagnostics

## 📋 Prerequisites

- Basic understanding of try-catch blocks
- Ability to read C++ code (no need to write complex C++)
- Familiarity with text search/replace (grep, VSCode search, etc.)

## 🚀 Getting Started

### Step 1: Pick a File

Files are organized by priority. Start with **Priority 1** (easiest):

**Priority 1: Core Engine Files** (Good first issues)

- `src/MayaFlux/Core/Engine.cpp`
- `src/MayaFlux/Core/Subsystem*.cpp`
- `src/MayaFlux/Core/Backends/**/*.cpp`

**Priority 2: Processing Systems**

- `src/MayaFlux/Nodes/**/*.cpp`
- `src/MayaFlux/Buffers/**/*.cpp`
- `src/MayaFlux/Vruta/**/*.cpp`

**Priority 3: Everything Else**

- `src/MayaFlux/Kakshya/**/*.cpp`
- `src/MayaFlux/Yantra/**/*.cpp`
- `examples/**/*.cpp`

### Step 2: Claim Your File

Comment on the tracking issue: "I'm working on `<filename>`" to avoid duplicate work.

## 🔄 Migration Patterns

### Pattern 1: Replace `std::cerr` with Logging

**Before:**

```cpp
std::cerr << "Failed to open audio device" << std::endl;
```

**After:**

```cpp
MF_ERROR(Component::Core, Context::AudioSubsystem, "Failed to open audio device");
```

**Decision Points:**

- Is this in a real-time audio callback? → Use `MF_RT_ERROR`
- Is this a warning vs error? → Choose appropriate severity
- Which component? → Look at namespace/file location
- Which context? → See Context Guide below

### Pattern 2: Replace `throw` with `error()`

**Before:**

```cpp
throw std::runtime_error("Buffer size cannot be zero");
```

**After:**

```cpp
error<std::runtime_error>(
    Component::Buffers,
    Context::Configuration,
    std::source_location::current(),
    "Buffer size cannot be zero");
```

**Decision Points:**

- Should this throw or just log? → See Decision Tree below
- What exception type? → Usually keep the same
- Which component? → Look at namespace/file location
- Which context? → See Context Guide below

### Pattern 3: Replace `throw` inside `catch` with `error_rethrow()`

**Before:**

```cpp
try {
    initialize_backend();
} catch (const std::exception& e) {
    std::cerr << "Backend init failed: " << e.what() << std::endl;
    throw;
}
```

**After:**

```cpp
try {
    initialize_backend();
} catch (const std::exception& e) {
    error_rethrow(Component::Core, Context::Init, "Backend init failed");
}
```

### Pattern 4: Replace `std::cout` debug output

**Before:**

```cpp
std::cout << "Processing buffer with " << num_samples << " samples" << std::endl;
```

**After:**

```cpp
MF_DEBUG(Component::Buffers, Context::BufferProcessing,
         "Processing buffer with {} samples", num_samples);
```

**Or remove entirely if it's temporary debug code!**

## 📊 Decision Tree: Throw vs Fatal vs Log

```
Is this in a real-time audio callback?
├─ YES → Use MF_RT_ERROR (never throw/fatal in RT context!)
└─ NO → Continue...

Is the program in an unrecoverable state?
├─ YES → Use fatal()
│   Examples: Out of memory, corrupted critical data, nullptr derefs
└─ NO → Continue...

Can the caller handle this error?
├─ YES → Use error<ExceptionType>()
│   Examples: Invalid config, file not found, validation failures
└─ NO → Use MF_ERROR() (just log, don't throw)
    Examples: Non-critical warnings, informational errors
```

## 🗺️ Context Guide

**Choose the context that matches *where* this code executes.**
Each log message must specify a `Context` to allow real-time safety checks, filtering, and debugging.

---

### Real-Time Contexts

*(Never block, allocate, or throw.)*

| Context            | When to Use                      | Examples                                      |
| ------------------ | -------------------------------- | --------------------------------------------- |
| `AudioCallback`    | Inside the audio callback thread | Sample generation, node graph evaluation      |
| `GraphicsCallback` | Inside the rendering callback    | Frame rendering, visual updates               |
| `Realtime`         | Generic real-time thread         | RT-safe control updates, timing-sensitive ops |

---

### Backend Contexts

| Context           | When to Use                          | Examples                                  |
| ----------------- | ------------------------------------ | ----------------------------------------- |
| `AudioBackend`    | Audio backend driver layer           | RtAudio, JACK, ASIO callbacks             |
| `GraphicsBackend` | Graphics backend driver layer        | Vulkan, OpenGL, or Metal API interactions |
| `CustomBackend`   | User-defined or experimental backend | Plugin or external backend integration    |

---

### GPU Contexts

| Context      | When to Use                  | Examples                            |
| ------------ | ---------------------------- | ----------------------------------- |
| `GPUCompute` | GPU compute operations       | GPGPU kernels, shader compute tasks |
| `Rendering`  | Rendering pipeline execution | Frame composition, draw calls       |

---

### Subsystem Contexts

| Context              | When to Use                      | Examples                             |
| -------------------- | -------------------------------- | ------------------------------------ |
| `AudioSubsystem`     | Audio subsystem orchestration    | Device enumeration, stream lifecycle |
| `GraphicsSubsystem`  | Graphics subsystem orchestration | Render pipeline, swapchain setup     |
| `WindowingSubsystem` | Windowing system integration     | GLFW, SDL, platform UI handling      |
| `CustomSubsystem`    | Custom or user-defined subsystem | Third-party integrations, plugins    |

---

### Processing Contexts

| Context               | When to Use                     | Examples                                         |
| --------------------- | ------------------------------- | ------------------------------------------------ |
| `NodeProcessing`      | Node graph processing           | Node evaluation, dependency resolution           |
| `BufferProcessing`    | Buffer transformations          | DSP chains, convolution, routing                 |
| `BufferManagement`    | Buffer allocation and lifecycle | Creation, recycling, metadata updates            |
| `CoroutineScheduling` | Coroutine or task scheduling    | `Vruta::TaskScheduler`, cooperative multitasking |
| `ContainerProcessing` | File or region-based data ops   | `Kakshya` file/stream/region parsing             |
| `ComputeMatrix`       | Mathematical or DSP compute     | `Yantra` operations, transforms, matrix algebra  |
| `ImageProcessing`     | Image operations                | Filters, frame transforms, texture updates       |
| `ShaderCompilation`   | GPU shader compilation          | GLSL/SPIR-V compilation, reflection              |

---

### Worker Contexts

| Context             | When to Use                          | Examples                                 |
| ------------------- | ------------------------------------ | ---------------------------------------- |
| `Worker`            | Background worker threads            | Deferred non-RT processing               |
| `AsyncIO`           | Asynchronous I/O operations          | Streaming, networking, background loads  |
| `FileIO`            | Filesystem read/write                | Project saves, sample loading            |
| `BackgroundCompile` | Background code or asset compilation | Caching, JIT builds, optimization passes |

---

### Lifecycle Contexts

| Context         | When to Use             | Examples                                    |
| --------------- | ----------------------- | ------------------------------------------- |
| `Init`          | Initialization routines | Engine, subsystem, or backend startup       |
| `Shutdown`      | Cleanup routines        | Graceful teardown, memory cleanup           |
| `Configuration` | Config/state changes    | Sample rate, buffer size, render resolution |

---

### User Interaction Contexts

| Context       | When to Use                | Examples                        |
| ------------- | -------------------------- | ------------------------------- |
| `UI`          | User interface thread      | GUI events, parameter updates   |
| `UserCode`    | User script or plugin code | Embedded scripts, live patches  |
| `Interactive` | Interactive shell / REPL   | Live coding, runtime evaluation |

---

### Coordination Contexts

| Context          | When to Use                      | Examples                                 |
| ---------------- | -------------------------------- | ---------------------------------------- |
| `CrossSubsystem` | Data coordination across domains | Audio ↔ Graphics sync, shared buffers    |
| `ClockSync`      | Clock synchronization            | `SampleClock` and `FrameClock` alignment |
| `EventDispatch`  | System-wide event routing        | Inter-module messaging, async dispatch   |

---

### Special Contexts

| Context   | When to Use                | Examples                                            |
| --------- | -------------------------- | --------------------------------------------------- |
| `Runtime` | General runtime            | Default context, fallback case                      |
| `Testing` | Testing or benchmarking    | Unit tests, performance profiles                    |
| `API`     | External API entry points  | Public-facing bindings, integrations                |
| `Unknown` | Unspecified or placeholder | Use only temporarily with `// TODO: verify context` |

---

### Default Choice

 **When in doubt, use `Context::Runtime` and add a comment:**
 `// TODO: verify context`

This ensures logs remain consistent and searchable while preserving real-time safety.

## 🧩 Component Guide

Components usually map to namespaces:

| Namespace/Directory | Component            |
| ------------------- | -------------------- |
| `Core::`            | `Component::Core`    |
| `Nodes::`           | `Component::Nodes`   |
| `Buffers::`         | `Component::Buffers` |
| `Vruta::`           | `Component::Vruta`   |
| `Kakshya::`         | `Component::Kakshya` |
| `Yantra::`          | `Component::Yantra`  |
| `IO::`          | `Component::IO`  |
| `Registry::`          | `Component::Registry`  |
| `Portal::`          | `Component::Portal`  |
| `examples/`         | `Component::USER`    |

## ✅ Checklist Before Submitting PR

- [ ] All `std::cerr`/`std::cout` replaced with appropriate logging
- [ ] All `throw` statements converted to `error()` or left as-is with comment
- [ ] Correct `Component::` chosen (matches namespace)
- [ ] Correct `Context::` chosen (matches execution context)
- [ ] Real-time contexts use `MF_RT_*` macros (no `error()`/`fatal()`)
- [ ] Code compiles without errors
- [ ] Ran existing tests (if any)
- [ ] Added `#include "MayaFlux/Journal/Archivist.hpp"` if needed

## 📝 PR Template

```markdown
### Logging Migration: [Filename]

**Files Changed:**

- `path/to/file.cpp`

**Summary:**

- Replaced X `std::cerr` with `MF_ERROR/MF_WARN`
- Replaced X `throw` with `error<>()`
- Replaced X `std::cout` with `MF_DEBUG` (or removed)

**Notes:**

- Used `Context::Runtime` for [specific cases] - please verify
- Left `throw` on line X - seems like existing API contract
- Question about [specific case]

**Testing:**

- [x] Code compiles
- [x] Existing tests pass
- [ ] Manually tested [if applicable]
```

## 🤔 When to Ask for Help

**ASK immediately if you're unsure about:**

- ❓ Should this throw or just log?
- ❓ Is this in a real-time context?
- ❓ Which Component/Context to use?
- ❓ Whether to keep existing throw behavior

**It's better to ask than guess!** Comment on your PR or the tracking issue.

## 🎓 Learning Resources

- Read `docs/Journal_System.md` for architecture overview
- Look at already-migrated files for examples
- Check enum definitions in `JournalEntry.hpp` for all options
- Search codebase for `MF_ERROR` to see usage patterns

## 🏆 Recognition

All contributors will be credited in:

- Git commit history
- `CONTRIBUTORS.md` file
- Release notes for the next version

Thank you for helping improve MayaFlux! 🎵
