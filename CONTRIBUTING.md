# Contributing to MayaFlux

Thank you for your interest in contributing to **MayaFlux**!
This project is built on the principles of open knowledge, equitable access, and collaborative development.
By participating, you agree to abide by the **GPLv3 license** and the guidelines below.

---

## 🧩 Contribution Philosophy

1. **Open Source First**
   All contributions must remain open and compatible with GPLv3.
   No proprietary or NDA-bound work can be merged.

2. **Shared Credit**
   All contributors are acknowledged in release notes, `CONTRIBUTORS.md`, and Git history.
   Attribution and citation are integral to how MayaFlux recognizes effort.

3. **Public Benefit**
   Contributions should advance the public, creative, or technical potential of digital media systems.
   This project exists to empower creators, not gatekeep technology.

4. **Technical Integrity**
   MayaFlux adheres to real-time safety, cross-platform correctness, and compositional clarity.
   PRs that violate these principles may be deferred until restructured.

---

## 🔧 Contribution Workflow

1. **Fork** the repository.
2. **Create a branch** for your feature, fix, or document.
3. **Commit cleanly** : prefer focused commits with descriptive messages.
4. **Submit a Pull Request (PR)** containing:

   * A summary of what was done and why
   * Notes on testing (especially for real-time or platform-critical changes)
   * References to any algorithms, papers, or external code used

PRs are reviewed for technical clarity, conceptual alignment, and code safety.

---

## 🛠️ Building and Testing

If you just want to use MayaFlux, use [Weave](https://github.com/MayaFlux/Weave). If you are modifying MayaFlux itself, build from source in this repo; see [`docs/Dev_Getting_Started.md`](docs/Dev_Getting_Started.md) for presets, targets, and the actual run loop.

Before opening a PR:

* Exercise your change in `src/user_project.hpp` or, for anything you want to keep around without cluttering that file, as its own file under `src/examples/`. This is the sandbox, not the review surface.
* If the change is warranted for regression coverage, add a real test under `tests/` (GoogleTest, gated by `MAYAFLUX_BUILD_TESTS`). Manual exercise in `user_project.hpp` is not a substitute for a test when one is warranted.
* State in the PR description what you tested and how, per the workflow above.

---

## 🍎 Wanted: macOS Platform Maintainer

This is the single highest-leverage contribution available right now.

macOS currently uses GLFW for windowing, the only platform still on it. Windows has a native Win32 backend, Linux has native Wayland. macOS needs the same treatment: a native windowing backend built on Cocoa/AppKit, following the same shape as `WIN32_BACKEND` and `WAYLAND_BACKEND` (see `cmake/defines.cmake`, `MAYAFLUX_WINDOWING_BACKEND`). CAMetalLayer integration into the existing Vulkan (MoltenVK) swapchain path is part of the same effort.

Beyond windowing, macOS as a whole needs an actual maintainer. The current author holds it together with a VM and no daily-driver access to real Apple hardware. If you have a Mac, know Cocoa/AppKit and Metal, and want ownership of a real subsystem rather than a starter task, open an issue tagged `platform-macos` or reach out directly before starting.

This is not a "good first issue." It requires:

* Working knowledge of Cocoa/AppKit windowing and event handling
* Familiarity with CAMetalLayer and how MoltenVK expects to receive it
* Willingness to own the backend going forward, not just land one PR and disappear
* Real hardware to test on; a VM is not sufficient for this work

---

## 🚀 Contribution Areas

MayaFlux welcomes contributions across several domains:

| Area                                | Description                                            | Reference                                      |
| ----------------------------------- | ------------------------------------------------------ | ---------------------------------------------- |
| **Core Development**                | Engine code, nodes, scheduling, DSP, graphics, runtime | Internal review required                       |
| **macOS Platform**                  | Native windowing, CAMetal, Cocoa/AppKit; see above     | `platform-macos` issue label                   |
| **Documentation & Tutorials**       | Guides, concept overviews, teaching materials          | `docs/`                                        |
| **Starter Tasks**                   | Logging cleanup, context tagging, code modernization   | [`docs/StarterTasks.md`](docs/StarterTasks.md) — flagged for review, may be stale |
| **Build Operations & Distribution** | CI/CD, installers, package manager recipes             | [`docs/BuildOps.md`](docs/BuildOps.md) — flagged for review, may be stale |
| **Research & Theory**               | Algorithmic or conceptual proposals                    | Open issue → Discussion thread                 |

---

**New to MayaFlux?** Start with [`docs/Dev_Getting_Started.md`](docs/Dev_Getting_Started.md) to get building, then look at open issues for current priorities. `StarterTasks.md` and `BuildOps.md` are under review and may not reflect current state.

---

## 🧱 Requirements for All PRs

* Code must **compile cleanly** on at least one supported platform.
* Documentation should be updated when behavior changes.
* Maintain **real-time safety** in audio and rendering contexts.
* Follow existing code style conventions.
* Include license headers where appropriate.

---

## 🤝 Communication & Etiquette

* Use Issues or Discussions for questions before starting major work.
* Be respectful and patient — we aim for rigor, not speed.
* Engage constructively; critical discourse is encouraged, hostility is not.
* Contributions are welcome regardless of background or experience.

---

## 🧠 AI-Assisted Contributions

MayaFlux recognizes that AI, in the hands of thoughtful and experienced practitioners, can be a powerful tool.

Like any instrument, its value depends on the person using it.

A master musician does not rely on the prestige of a rare instrument to create meaningful work. A Stradivarius may refine expression, but it does not replace authorship, intent, or understanding. The same applies to AI: it can extend capability, but it cannot substitute responsibility.

**Policy:**

* AI-assisted contributions are **welcome**.
* AI may be used for ideation, drafting, refactoring, or bulk generation.
* However, **all contributions must be authored, reviewed, and submitted by a human**.

The individual submitting a PR is fully responsible for:

* The correctness and safety of the code
* Compliance with GPLv3 and licensing requirements
* Alignment with MayaFlux's architectural and conceptual principles

**Non-negotiable constraint:**

* Contributions **must not be authored or signed by autonomous agents**.
* No bot-driven commits, AI identities, or automated PR generation without human ownership.

If you submit work, you are the author.

---

## ⚖️ Legal & Licensing

By contributing, you affirm that:

* You have the right to share your contribution under GPLv3.
* Your code/documentation is free from third-party restrictions.

This ensures MayaFlux remains open, sustainable, and publicly beneficial.

---

## 🪜 Where to Start

If you're new, begin with:

* [`docs/Dev_Getting_Started.md`](docs/Dev_Getting_Started.md) — build from source, targets, presets, the actual run loop
* Open issues, especially [`platform-macos`](https://github.com/MayaFlux/MayaFlux/labels/platform-macos) if you have a Mac and want to own something real
* [`docs/StarterTasks.md`](docs/StarterTasks.md) and [`docs/BuildOps.md`](docs/BuildOps.md) — under review, may be stale

---

*(For questions or coordination, open an issue with the appropriate label, e.g. `platform-macos`, `build-ops`, `doc`, `feature`, or `discussion`.)*
