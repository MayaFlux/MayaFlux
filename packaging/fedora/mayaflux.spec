Name:           mayaflux
Version:        0.1.2
Release:        1%{?dist}
Summary:        Modern C++ framework for real-time graphics and audio with JIT compilation

License:        GPLv3
URL:            https://github.com/MayaFlux/MayaFlux
Source0:        https://github.com/MayaFlux/MayaFlux/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

ExclusiveArch:  x86_64

%global __requires_exclude ^pkgconfig\\(

# ALL dependencies are both build AND runtime for MayaFlux
BuildRequires:  gcc-c++ >= 15
BuildRequires:  clang >= 21
BuildRequires:  llvm >= 21
BuildRequires:  llvm-devel >= 21
BuildRequires:  llvm-libs >= 21
BuildRequires:  clang-devel >= 21
BuildRequires:  cmake >= 3.25
BuildRequires:  ninja-build
BuildRequires:  pkgconfig
BuildRequires:  rtaudio-devel
BuildRequires:  glfw-devel >= 3.4
BuildRequires:  glm-devel
BuildRequires:  eigen3-devel
BuildRequires:  spirv-headers-devel
BuildRequires:  spirv-cross-devel
BuildRequires:  spirv-tools
BuildRequires:  vulkan-headers
BuildRequires:  vulkan-loader
BuildRequires:  vulkan-loader-devel
BuildRequires:  vulkan-tools
BuildRequires:  vulkan-validation-layers
BuildRequires:  ffmpeg-free-devel
BuildRequires:  stb-devel
BuildRequires:  magic_enum-devel
BuildRequires:  tbb-devel
BuildRequires:  gtest-devel
BuildRequires:  libshaderc-devel
BuildRequires:  glslc
BuildRequires:  wayland-devel
BuildRequires:  git

# Runtime = BuildRequires (all needed for live coding/JIT)
Requires:       gcc-c++ >= 15
Requires:       clang >= 21
Requires:       llvm >= 21
Requires:       llvm-devel >= 21
Requires:       llvm-libs >= 21
Requires:       clang-devel >= 21
Requires:       cmake >= 3.25
Requires:       pkgconfig
Requires:       rtaudio-devel
Requires:       glfw-devel >= 3.4
Requires:       glm-devel
Requires:       eigen3-devel
Requires:       spirv-headers-devel
Requires:       spirv-cross-devel
Requires:       spirv-tools
Requires:       vulkan-headers
Requires:       vulkan-loader
Requires:       vulkan-loader-devel
Requires:       vulkan-tools
Requires:       vulkan-validation-layers
Requires:       ffmpeg-free-devel
Requires:       stb-devel
Requires:       magic_enum-devel
Requires:       tbb-devel
Requires:       gtest-devel
Requires:       libshaderc-devel
Requires:       glslc
Requires:       wayland-devel

Provides:       mayaflux = %{version}-%{release}
Conflicts:      mayaflux-dev

%description
MayaFlux is a modern C++23 framework for real-time graphics and audio processing.

MayaFlux reimagines digital creative computing by moving beyond analog hardware 
metaphors toward truly digital-first paradigms. It treats audio, visual, and 
control data as unified numerical streams processed through lock-free node graphs, 
C++20 coroutines for temporal coordination, and grammar-driven operation pipelines.

Features:
- Unified audio-visual-control data processing (digital-first paradigm)
- Lock-free real-time audio subsystem with sample-accurate timing
- Vulkan-based graphics pipeline (not just visualizers - full creative tool)
- Live C++ coding via LLVM JIT compilation (Lila subsystem)
- Cross-modal data flow (audio drives physics, logic gates generate rhythm)
- Coroutine-based temporal coordination

NOTE: All development dependencies are required at runtime for JIT compilation
and live coding features. This is intentional.

%prep
%autosetup -n MayaFlux-%{version}

%build
%cmake -G Ninja \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_STANDARD=23 \
    -DCMAKE_INSTALL_LIBDIR=%{_libdir} \
    -DMAYAFLUX_PORTABLE=OFF \
    -DMAYAFLUX_BUILD_TESTS=OFF

%cmake_build

%install
%cmake_install

if [ "%{_libdir}" != "/usr/lib" ]; then
    mkdir -p %{buildroot}%{_libdir}
    mv %{buildroot}/usr/lib/*.so* %{buildroot}%{_libdir}/ 2>/dev/null || true
    mv %{buildroot}/usr/lib/pkgconfig %{buildroot}%{_libdir}/ 2>/dev/null || true
    mv %{buildroot}/usr/lib/cmake %{buildroot}%{_libdir}/ 2>/dev/null || true
fi

mkdir -p %{buildroot}%{_sysconfdir}/profile.d
cat > %{buildroot}%{_sysconfdir}/profile.d/mayaflux.sh << 'EOF'
export MAYAFLUX_ROOT="%{_prefix}"
export CMAKE_PREFIX_PATH="%{_prefix}:${CMAKE_PREFIX_PATH}"
EOF

%check
%{buildroot}%{_bindir}/lila_server --version 2>&1 | grep -q "MayaFlux\|version" || :

%files
%license LICENSE
%doc README.md
%{_bindir}/lila_server
%{_libdir}/libMayaFluxLib.so*
%{_libdir}/libLila.so*
%{_includedir}/MayaFlux/
%{_includedir}/Lila/
%{_datadir}/MayaFlux/
%{_libdir}/pkgconfig/*.pc
%{_libdir}/cmake/MayaFlux/
%config(noreplace) %{_sysconfdir}/profile.d/mayaflux.sh

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%changelog
* Sun Jan 18 2026 MayaFlux Collective <mayafluxcollective@proton.me> - 0.1.2-1
- Initial stable release
- Full source build with C++23 support
- All development dependencies included for JIT/live coding
