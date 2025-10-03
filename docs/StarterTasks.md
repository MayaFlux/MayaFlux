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

**Choose the context that matches WHERE this code executes:**

| Context               | When to Use                         | Examples                                               |
| --------------------- | ----------------------------------- | ------------------------------------------------------ |
| `AudioCallback`       | Inside audio processing callback    | Buffer processing, sample generation                   |
| `GraphicsCallback`    | Inside rendering callback           | Frame rendering, visual updates                        |
| `Realtime`            | Any real-time processing context    | Updates to system components via RT scheduled entities |
| `AudioBackend`        | Audio backend operations            | RtAudio, JACK, ASIO driver calls                       |
| `GraphicsBackend`     | Graphics backend operations         | Vulkan/OpenGL driver calls                             |
| `CustomBackend`       | User-defined backend                | Plugin-defined backend                                 |
| `AudioSubsystem`      | Audio subsystem orchestration       | Device management, stream setup                        |
| `GraphicsSubsystem`   | Graphics subsystem orchestration    | Rendering pipeline, swapchain setup                    |
| `WindowingSubsystem`  | Windowing system operations         | GLFW, SDL                                              |
| `CustomSubsystem`     | User-defined subsystem              | Plugin-defined subsystem                               |
| `NodeProcessing`      | Node graph operations               | Node creation, evaluation, graph scheduling            |
| `BufferProcessing`    | Buffer operations                   | Allocation, processing chains                          |
| `CoroutineScheduling` | Coroutine/task scheduling           | Temporal coordination, `Vruta::TaskScheduler`          |
| `ContainerProcessing` | Container/file/region operations    | Kakshya file/stream/region processing                  |
| `ComputeProcessing`   | Compute/DSP/matrix operations       | Yantra algorithms, transforms                          |
| `Worker`              | Background worker thread tasks      | Non-RT scheduled work                                  |
| `AsyncIO`             | Asynchronous I/O                    | File loading, network streaming                        |
| `BackgroundCompile`   | Background compilation/optimization | JIT tasks, preprocessing                               |
| `Init`                | Engine/subsystem initialization     | Startup routines                                       |
| `Shutdown`            | Engine/subsystem shutdown           | Cleanup, teardown                                      |
| `Configuration`       | Parameter/configuration changes     | Buffer size, sample rate, graphics options             |
| `UI`                  | User interface thread               | UI events, rendering                                   |
| `UserCode`            | User script/plugin execution        | Scripting environments, dynamic plugins                |
| `Interactive`         | Interactive shell or REPL           | Live coding, REPL                                      |
| `CrossSubsystem`      | Cross-subsystem coordination        | Data sharing between audio/graphics                    |
| `ClockSync`           | Clock synchronization               | SampleClock ↔ FrameClock                              |
| `EventDispatch`       | Event dispatching and coordination  | System-wide events                                     |
| `Runtime`             | General runtime operations          | Default catch-all                                      |
| `Testing`             | Testing/benchmarking                | Unit tests, perf benchmarks                            |
| `Unknown`             | Unknown or unspecified              | Use only if unclear (add TODO to verify)               |

**When in doubt, use `Context::Runtime`** and comment `// TODO: verify context`

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
