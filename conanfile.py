from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
import os


class MayafluxConan(ConanFile):
    name = "mayaflux"
    version = "0.1.0"
    package_type = "application"

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_lila": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "with_lila": True,
        # Dependency versions and options
        "ffmpeg:shared": True,
        "llvm:shared": True,
        "rtaudio:shared": True,
        "glfw:shared": True,
        "vulkan-loader:shared": True,
    }

    # Metadata
    license = "MIT"
    author = "MayaFlux Team"
    url = "https://github.com/your/mayaflux"
    description = "Creative coding framework with live programming"
    topics = ("creative-coding", "audio", "graphics", "live-programming")

    # Exports for Conan to use
    exports_sources = "CMakeLists.txt", "cmake/*", "src/*", "tests/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        # On Windows, default to static for some libs to reduce DLL hell
        if self.settings.os == "Windows":
            self.options["ffmpeg"].shared = False
            self.options["rtaudio"].shared = False

    def configure(self):
        if not self.options.with_lila:
            # If Lila is disabled, we don't need LLVM
            self.options["llvm"].shared = False

    def requirements(self):
        # Core multimedia dependencies
        self.requires("rtaudio/5.2.0")
        self.requires("eigen/3.4.0")
        self.requires("magic_enum/0.9.3")
        self.requires("glfw/3.3.8")

        # FFmpeg components
        self.requires("ffmpeg/5.1.2")

        # Vulkan (loader and headers)
        self.requires("vulkan-loader/1.3.250.0")
        self.requires("vulkan-headers/1.3.250.0")

        # Parallel computing backends
        if self.settings.os == "Windows":
            self.requires("ms-gsl/4.0.0")  # For PPL on Windows
        else:
            self.requires("tbb/2020.3")  # For TBB on Linux/macOS

        # Formatting library (for macOS compatibility)
        self.requires("fmt/10.0.0")

        # Testing
        self.requires("gtest/1.14.0")

        # Lila dependencies (conditional)
        if self.options.with_lila:
            self.requires("llvm/17.0.6")
            # Clang is part of LLVM package in Conan

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)

        # Pass options to CMake
        tc.variables["BUILD_LILA"] = self.options.with_lila
        tc.variables["MAYAFLUX_USE_CONAN"] = True

        # Platform-specific settings
        if self.settings.os == "Windows":
            tc.variables["USE_PPL_BACKEND"] = True
        else:
            tc.variables["USE_TBB_BACKEND"] = True

        # Use fmt on macOS for format compatibility
        if self.settings.os == "Macos":
            tc.variables["MAYAFLUX_USE_FMT"] = True

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

        # Copy additional files that Conan might need
        copy(
            self,
            "*.h",
            os.path.join(self.source_folder, "src"),
            os.path.join(self.package_folder, "include"),
        )
        copy(
            self,
            "*.hpp",
            os.path.join(self.source_folder, "src"),
            os.path.join(self.package_folder, "include"),
        )

    def package_info(self):
        self.cpp_info.libs = ["MayaFluxLib"]

        if self.options.with_lila:
            self.cpp_info.components["lila"].libs = ["Lila"]
            self.cpp_info.components["lila"].requires = [
                "llvm::llvm",
                "clang::clang",
                "mayaflux::mayaflux",
            ]

        # Set runtime paths for dynamic libraries
        if self.settings.os == "Windows":
            self.cpp_info.bindirs = ["bin"]
        else:
            self.cpp_info.libdirs = ["lib"]
