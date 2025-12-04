@{
    SystemTools        = @{
        CMake    = @{
            WingetId = 'Kitware.CMake'
            Verify   = 'cmake'  # Command to check
        }
        Git      = @{
            WingetId = 'Git.Git'
            Verify   = 'git'
        }
        Ninja    = @{
            WingetId = 'Ninja-build.Ninja'
            Verify   = 'ninja'
        }
        SevenZip = @{
            WingetId = '7zip.7zip'
            Verify   = 'C:\Program Files\7-Zip\7z.exe'  # File path to check
        }
    }

    BinaryPackages     = @{
        LLVM      = @{
            InstallRoot = 'C:\Program Files\LLVM_Libs'
            Url         = 'https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.3/clang+llvm-21.1.3-x86_64-pc-windows-msvc.tar.xz'
            Verify      = 'C:\Program Files\LLVM_Libs\lib\LLVMCore.lib'
        }

        VulkanSDK = @{
            InstallRoot = 'C:\VulkanSDK'
            Url         = 'https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe'
            InstallArgs = @('/S')
            Verify      = 'C:\VulkanSDK\*\Include\vulkan\vulkan.h'  # Wildcard for version
        }

        FFmpeg    = @{
            InstallRoot = 'C:\Program Files\FFmpeg'
            Url         = 'https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-full-shared.7z'
            Verify      = 'C:\Program Files\FFmpeg\bin\ffmpeg.exe'
        }

        GLFW      = @{
            InstallRoot = 'C:\Program Files\GLFW'
            Url         = 'https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip'
            Verify      = 'C:\Program Files\GLFW\include\GLFW\glfw3.h'
        }
    }

    HeaderOnlyPackages = @{
        Eigen3    = @{
            InstallRoot = 'C:\Program Files\Eigen3'
            Url         = 'https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip'
            Verify      = 'C:\Program Files\Eigen3\Eigen\Dense'
        }

        GLM = @{
            InstallRoot   = 'C:\Program Files\glm'
            Url           = 'https://github.com/g-truc/glm/releases/download/1.0.2/glm-1.0.2.zip'
            HeaderSubpath = 'glm'
            TargetSubdir  = 'glm'
            Verify        = 'C:\Program Files\glm\include\glm\glm.hpp'
        }

        STB       = @{
            InstallRoot   = 'C:\Program Files\stb'
            Url           = 'https://github.com/nothings/stb/archive/master.zip'
            HeaderPattern = '*.h'
            TargetSubdir  = 'stb'
            Verify        = 'C:\Program Files\stb\include\stb\stb_image.h'
        }

        MagicEnum = @{
            InstallRoot   = 'C:\Program Files\magic_enum'
            Url           = 'https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.5.zip'
            HeaderSubpath = 'include\magic_enum'
            Verify        = 'C:\Program Files\magic_enum\include\magic_enum.hpp'
        }
    }

    SourcePackages     = @{
        RtAudio = @{
            InstallRoot  = 'C:\Program Files\RtAudio'
            GitUrl       = 'https://github.com/thestk/rtaudio.git'
            GitBranch    = '6.0.1'
            Verify       = 'C:\Program Files\RtAudio\include\rtaudio\RtAudio.h'
            CMakeOptions = @{
                CMAKE_BUILD_TYPE      = 'Release'
                RTAUDIO_BUILD_TESTING = 'OFF'
                RTAUDIO_API_WASAPI    = 'ON'
                RTAUDIO_API_DS        = 'ON'
            }
            Generator    = 'Visual Studio 17 2022'
        }

        LibXml2 = @{
            InstallRoot = 'C:\Program Files\LibXml2'
            Url         = 'https://download.gnome.org/sources/libxml2/2.15/libxml2-2.15.0.tar.xz'
            Verify      = 'C:\Program Files\LibXml2\include\libxml\xmlversion.h.in'
            HeadersOnly = $true
        }
    }

    SpecialPackages    = @{
        VisualStudio = @{
            Verify  = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
            Message = 'Visual Studio with C++ workload required. Download from https://visualstudio.microsoft.com/'
        }
    }

    MayaFlux           = @{
        InstallRoot    = 'C:\MayaFlux'
        Subdirectories = @('include', 'lib', 'bin')
    }
}
