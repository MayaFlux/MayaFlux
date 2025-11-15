@{
    SystemTools        = @{
        CMake    = @{
            WingetId = "Kitware.CMake"
            Verify   = { Test-Command "cmake" }
        }
        Git      = @{
            WingetId = "Git.Git"
            Verify   = { Test-Command "git" }
        }
        Ninja    = @{
            WingetId = "Ninja-build.Ninja"
            Verify   = { Test-Command "ninja" }
        }
        SevenZip = @{
            WingetId = "7zip.7zip"
            Verify   = { Test-Path "${env:ProgramFiles}\7-Zip\7z.exe" }
        }
    }

    BinaryPackages     = @{
        LLVM      = @{
            InstallRoot = "C:\Program Files\LLVM_Libs"
            Url         = "https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.3/clang+llvm-21.1.3-x86_64-pc-windows-msvc.tar.xz"
            Verify      = { Test-Path "C:\Program Files\LLVM_Libs\lib\LLVMCore.lib" }
        }

        VulkanSDK = @{
            InstallRoot = "C:\VulkanSDK"
            Url         = "https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe"
            InstallArgs = @("/S")
            Verify      = {
                $versionDir = Get-ChildItem "C:\VulkanSDK" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
                $versionDir -and (Test-Path "$($versionDir.FullName)\Include\vulkan\vulkan.h")
            }
        }

        FFmpeg    = @{
            InstallRoot = "C:\Program Files\FFmpeg"
            Url         = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-full-shared.7z"
            Verify      = { Test-Path "C:\Program Files\FFmpeg\bin\ffmpeg.exe" }
        }

        GLFW      = @{
            InstallRoot = "C:\Program Files\GLFW"
            Url         = "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip"
            Verify      = { Test-Path "C:\Program Files\GLFW\include\GLFW\glfw3.h" }
        }
    }

    HeaderOnlyPackages = @{
        Eigen3    = @{
            InstallRoot = "C:\Program Files\Eigen3"
            Url         = "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip"
            Verify      = { Test-Path "C:\Program Files\Eigen3\Eigen\Dense" }
        }

        GLM       = @{
            InstallRoot   = "C:\Program Files\glm"
            Url           = "https://github.com/g-truc/glm/releases/download/1.0.2/glm-1.0.2.zip"
            HeaderSubpath = "glm"
            Verify        = { Test-Path "C:\Program Files\glm\include\glm\glm.hpp" }
        }

        STB       = @{
            InstallRoot   = "C:\Program Files\stb"
            Url           = "https://github.com/nothings/stb/archive/master.zip"
            HeaderPattern = "*.h"
            TargetSubdir  = "include\stb"
            Verify        = { Test-Path "C:\Program Files\stb\include\stb\stb_image.h" }
        }

        MagicEnum = @{
            InstallRoot   = "C:\Program Files\magic_enum"
            Url           = "https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.5.zip"
            HeaderSubpath = "include"
            Verify        = { Test-Path "C:\Program Files\magic_enum\include\magic_enum.hpp" }
        }
    }

    SourcePackages     = @{
        RtAudio = @{
            InstallRoot  = "C:\Program Files\RtAudio"
            GitUrl       = "https://github.com/thestk/rtaudio.git"
            GitBranch    = "6.0.1"
            Verify       = { Test-Path "C:\Program Files\RtAudio\include\rtaudio\RtAudio.h" }
            CMakeOptions = @{
                CMAKE_BUILD_TYPE      = "Release"
                RTAUDIO_BUILD_TESTING = "OFF"
                RTAUDIO_API_WASAPI    = "ON"
                RTAUDIO_API_DS        = "ON"
            }
            Generator    = "Visual Studio 17 2022"
        }

        LibXml2 = @{
            InstallRoot = "C:\Program Files\LibXml2"
            Url         = "https://download.gnome.org/sources/libxml2/2.15/libxml2-2.15.0.tar.xz"
            Verify      = { Test-Path "C:\Program Files\LibXml2\include\libxml\xmlversion.h" }
            HeadersOnly = $true
        }
    }

    SpecialPackages    = @{
        VisualStudio = @{
            Verify  = {
                $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
                Test-Path $vswhere
            }
            Message = "Visual Studio with C++ workload required. Download from https://visualstudio.microsoft.com/"
        }
    }

    MayaFlux           = @{
        InstallRoot    = "C:\MayaFlux"
        Subdirectories = @("include", "lib", "bin")
    }
}
