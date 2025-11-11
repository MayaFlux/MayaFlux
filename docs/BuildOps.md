# 🧱 Build Operations & Distribution

*(See also: [CONTRIBUTING.md](../CONTRIBUTING.md) for general contribution policies.)*

MayaFlux is at the stage where **building from source is expected**.
Our priority is to make that experience reliable across all platforms — and then automate it.

This document focuses on how contributors can improve build pipelines, packaging, and user distribution.

---

## ✅ Current State

**Complete:**

* Dependency setup scripts (`setup_macos.sh`, `setup_windows.ps1`, `setup_linux.sh`)
* CMake infrastructure (cross-platform, tested locally)
* Core codebase (audio, graphics, live coding systems)

**Missing / Needs Work:**

* Documented build steps for each OS
* CI/CD workflows that generate binaries
* Cross-platform validation
* Installer packaging (DMG, MSI, AppImage)
* Distribution via Homebrew, vcpkg, AUR, etc.

---

## 🧭 Phase 1 — Documentation & First-Time Experience

**Goal:** Let anyone build MayaFlux successfully from a clean system.

**Contributors:** Write build guides for macOS, Windows, and Linux using the provided setup scripts.

*(Full table unchanged — your original text is already perfect here.)*

---

## ⚙️ Phase 2 — CI/CD & Binary Generation

**Needed:** GitHub Actions workflows that build and package binaries.

**Not needed:** Performance optimization. Complex caching. Internal CI tweaking.

**Note:** You already have partial CI. This completes it for distribution.

#### macOS Binary Release
- **What:** GitHub Actions workflow that:
  1. Runs `setup_macos.sh`
  2. Builds via CMake
  3. Packages as `.tar.gz` and `.zip`
  4. Uploads to GitHub Releases
- **Triggers:** On tag (`v0.1.0`)
- **Effort:** 4-6 hours (workflow + testing)
- **Files to create:** `.github/workflows/build-macos-release.yml`

**Success criteria:**
- Push tag `v0.1.0`
- Workflow runs automatically
- `MayaFlux-0.1.0-macos.tar.gz` appears in Releases
- User can download, extract, run binaries

#### Windows Binary Release
- **What:** GitHub Actions workflow that:
  1. Runs `setup_windows.ps1`
  2. Builds via CMake (Visual Studio generator)
  3. Packages as `.zip`
  4. Uploads to Releases
- **Challenges:** MSI installer would be nice but complex; simple `.zip` is fine to start
- **Effort:** 5-7 hours
- **Files:** `.github/workflows/build-windows-release.yml`

#### Linux AppImage
- **What:** GitHub Actions workflow that:
  1. Builds in Ubuntu container
  2. Creates AppImage (single executable file)
  3. Uploads to Releases
- **Benefits:** Works on any Linux distro, no dependencies
- **Effort:** 6-8 hours (AppImage integration is finicky)
- **Files:** `.github/workflows/build-linux-appimage.yml`

**How to contribute:**
1. Copy your local build steps into a GitHub Actions YAML
2. Test by pushing to a branch
3. Fix environment differences (usually dependency paths)
4. When working, submit PR with workflow file

---

## 📦 Phase 3 — Package Manager Integration

**Needed:** Formulas/recipes for Homebrew, vcpkg, maybe Arch AUR.

**Not needed:** Vulkan SDK packaging. Dependency management. (Those are handled by existing managers.)

#### Homebrew Formula
- **What:** Ruby script (`Formula/mayaflux.rb`) that:
  1. Downloads source tarball
  2. Runs `cmake` configure
  3. Builds
  4. Installs to `$(brew --prefix)/opt/mayaflux`
- **Effort:** 4-6 hours
- **Impact:** `brew install mayaflux` works

**How to contribute:**
1. Create local Homebrew tap
2. Write formula using our CMakeLists.txt
3. Test locally: `brew install ./Formula/mayaflux.rb`
4. Submit to Homebrew core or maintain as third-party tap
5. PR to main repo with formula + instructions

#### vcpkg Port
- **What:** vcpkg manifest (`ports/mayaflux/`)
- **Effort:** 3-4 hours
- **Impact:** Game developers can `vcpkg install mayaflux`

#### Arch AUR
- **What:** PKGBUILD file for Arch User Repository
- **Effort:** 2-3 hours
- **Impact:** Arch users: `yay mayaflux`

---

## 🧰 Phase 4 — Distribution Packaging

**Needed:** Installers that handle:
- File organization
- Symlinks/PATH setup
- Post-install configuration

**Not needed:** Code signing (future). Auto-updater (future).

#### macOS DMG Installer
- **What:** Disk image with drag-to-Applications installer
- **Effort:** 3-4 hours
- **Files:** `packaging/create-dmg.sh`
- **Result:** Download → Mount → Drag folder → Done

#### Windows MSI Installer
- **What:** Standard Windows installer (.msi)
- **Tools:** WiX Toolset or similar
- **Effort:** 6-8 hours
- **Impact:** Professional distribution channel

#### Linux Installation Script
- **What:** Simple shell script that:
  1. Extracts AppImage/tarball
  2. Creates `/opt/mayaflux`
  3. Symlinks to `/usr/local/bin` or `~/.local/bin`
  4. Prints post-install instructions
- **Effort:** 2-3 hours

---
