@{
    SystemTools        = @{
        CMake    = @{
            WingetId = 'Kitware.CMake'
            Verify   = 'cmake'
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
            Verify   = 'C:\Program Files\7-Zip\7z.exe'
        }
    }

    WingetPackages = @{
        FFmpeg = @{
            WingetId = 'Gyan.FFmpeg'
            Verify   = 'ffmpeg'
        }
        VulkanSDK = @{
            WingetId = 'KhronosGroup.VulkanSDK'
            Verify   = 'C:\VulkanSDK\*\Include\vulkan\vulkan.h'
        }
    }

    BinaryPackages = @{
        LLVM = @{
            Version      = "21.1.8"
            InstallRoot  = "C:\Program Files\LLVM_Libs\21.1.8"
            Url          = "https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.8/clang+llvm-21.1.8-x86_64-pc-windows-msvc.tar.xz"
            Verify       = "C:\Program Files\LLVM_Libs\21.1.8\lib\LLVMCore.lib"
        }
    }

    HeaderOnlyPackages = @{}

    SourcePackages     = @{}

    VcpkgPackages = @{
        glm        = @{}
        eigen3     = @{}
        "magic-enum" = @{}
        stb        = @{}
        hidapi     = @{}
        glfw3      = @{}
        rtaudio    = @{}
        rtmidi     = @{}
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
