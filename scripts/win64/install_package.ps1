# install_package.ps1 - Generic package installation engine

param(
    [Parameter(Mandatory = $true)]
    [string]$PackagesFile = "packages.psd1",

    [Parameter(Mandatory = $false)]
    [string[]]$Only,  # Install only specific packages

    [Parameter(Mandatory = $false)]
    [switch]$SkipEnvSetup  # Skip environment variable setup
)

$ErrorActionPreference = "Stop"

# ===========================
# UTILITY FUNCTIONS
# ===========================

function Test-Command($cmd) {
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
    
    if (Get-Command $cmd -ErrorAction SilentlyContinue) {
        return $true
    }
    
    try {
        & $cmd --version *>$null
        return $true
    } catch {
        return $false
    }
}

function Add-ToSystemPath($path) {
    $current = [Environment]::GetEnvironmentVariable("Path", "Machine")
    if ($current -notmatch [Regex]::Escape($path)) {
        [Environment]::SetEnvironmentVariable("Path", "$current;$path", "Machine")
        Write-Host "  + Added to PATH: $path" -ForegroundColor Green
    }
}

function Add-ToSystemLib($path) {
    $current = [Environment]::GetEnvironmentVariable("LIB", "Machine")
    if (-not $current) { $current = "" }
    if ($current -notmatch [Regex]::Escape($path)) {
        $newLib = if ($current -eq "") { $path } else { "$current;$path" }
        [Environment]::SetEnvironmentVariable("LIB", $newLib, "Machine")
        Write-Host "  + Added to LIB: $path" -ForegroundColor Green
    }
}

function Add-ToSystemInclude($path) {
    $current = [Environment]::GetEnvironmentVariable("INCLUDE", "Machine")
    if (-not $current) { $current = "" }
    if ($current -notmatch [Regex]::Escape($path)) {
        $newInclude = if ($current -eq "") { $path } else { "$current;$path" }
        [Environment]::SetEnvironmentVariable("INCLUDE", $newInclude, "Machine")
        Write-Host "  + Added to INCLUDE: $path" -ForegroundColor Green
    }
}

function Invoke-WithRetry($action, $maxRetries = 3) {
    $attempt = 0
    while ($attempt -lt $maxRetries) {
        try {
            & $action
            return
        }
        catch {
            $attempt++
            if ($attempt -ge $maxRetries) { throw }
            Write-Warning "Attempt $attempt failed, retrying..."
            Start-Sleep -Seconds 2
        }
    }
}

function Expand-ArchiveSafe($archivePath, $destination) {
    $sevenZip = "${env:ProgramFiles}\7-Zip\7z.exe"
    if (-not (Test-Path $sevenZip)) { 
        throw "7-Zip required for extractions (install via: winget install 7zip.7zip)"
    }
    
    if (-not (Test-Path $archivePath)) {
        throw "Archive not found: $archivePath"
    }
    
    $archiveSize = [math]::Round((Get-Item $archivePath).Length / 1MB, 2)
    Write-Host "  [EXTRACT] Processing $archiveSize MB archive: $(Split-Path $archivePath -Leaf)" -ForegroundColor Yellow
    
    if (Test-Path $destination) { 
        Remove-Item $destination -Recurse -Force -ErrorAction SilentlyContinue 
    }
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    
    $ext = [System.IO.Path]::GetExtension($archivePath).ToLower()
    
    if ($archivePath -match "\.tar\.(xz|gz|bz2)$") {
        Write-Host "  [EXTRACT] Detected double-compressed archive (.tar.$($matches[1]))" -ForegroundColor Cyan
        
        $tempDir = Join-Path $env:TEMP "MayaFlux-extract-$(Get-Random)"
        New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
        
        Write-Host "  [EXTRACT] Step 1/2: Decompressing outer layer..." -ForegroundColor Yellow
        $tarFile = Join-Path $tempDir ([System.IO.Path]::GetFileNameWithoutExtension($archivePath))
        
        Start-Process -FilePath $sevenZip -ArgumentList "x `"$archivePath`" `"-o$tempDir`" -y -mmt=on" -NoNewWindow -Wait
        
        if (-not (Test-Path $tarFile)) {
            Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
            throw "Failed to decompress outer layer - .tar file not created: $tarFile"
        }
        
        Write-Host "  [EXTRACT] Step 2/2: Extracting tar archive..." -ForegroundColor Yellow
        
        Start-Process -FilePath $sevenZip -ArgumentList "x `"$tarFile`" `"-o$destination`" -y -mmt=on" -NoNewWindow -Wait
        
        Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
        
    } else {
        Write-Host "  [EXTRACT] Extracting with 7-Zip..." -ForegroundColor Yellow
        
        Start-Process -FilePath $sevenZip -ArgumentList "x `"$archivePath`" `"-o$destination`" -y -mmt=on" -NoNewWindow -Wait
    }
    
    $extractedFiles = Get-ChildItem $destination -Recurse -File -ErrorAction SilentlyContinue
    $fileCount = ($extractedFiles | Measure-Object).Count
    
    if ($fileCount -eq 0) {
        throw "Extraction produced no files - archive may be corrupted or extraction failed"
    }
    
    Write-Host "  [OK] Extracted $fileCount files" -ForegroundColor Green
}

# ===========================
# DOWNLOAD ACCELERATION
# ===========================

function Invoke-FastDownload {
    param(
        [string]$Url,
        [string]$OutFile
    )
    
    $OutFile = [System.IO.Path]::GetFullPath($OutFile)
    $directory = Split-Path $OutFile -Parent
    
    if (-not (Test-Path $directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    
    if (Test-Path $OutFile) {
        Remove-Item $OutFile -Force -ErrorAction SilentlyContinue
    }
    
    Write-Host "  [DOWNLOAD] Starting download..." -ForegroundColor Cyan
    
    $startTime = Get-Date
    $lastUpdate = Get-Date
    $lastBytes = 0
    
    $request = [System.Net.HttpWebRequest]::Create($Url)
    $request.Method = "GET"
    $request.UserAgent = "PowerShell"
    
try {
        $response = $request.GetResponse()
        $totalBytes = $response.ContentLength
        $responseStream = $response.GetResponseStream()
        $fileStream = [System.IO.File]::Create($OutFile)
        
        $buffer = New-Object byte[] 65536
        $totalRead = 0
        
        while (($read = $responseStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $fileStream.Write($buffer, 0, $read)
            $totalRead += $read
            
            $now = Get-Date
            if (($now - $lastUpdate).TotalMilliseconds -ge 5000) {
                $elapsed = ($now - $lastUpdate).TotalSeconds
                $speed = ($totalRead - $lastBytes) / $elapsed / 1MB
                $percent = [math]::Round(($totalRead / $totalBytes) * 100, 0)
                
                $receivedMB = $totalRead / 1MB
                $totalMB = $totalBytes / 1MB
                
                if ($speed -gt 0) {
                    $remaining = $totalBytes - $totalRead
                    $eta = [math]::Round($remaining / ($speed * 1MB))
                    Write-Host ("`r  [DOWNLOAD] {0:F1} MB / {1:F1} MB ({2}%) @ {3:F1} MB/s ETA:{4}s     " -f $receivedMB, $totalMB, $percent, $speed, $eta) -NoNewline -ForegroundColor Cyan
                } else {
                    Write-Host ("`r  [DOWNLOAD] {0:F1} MB / {1:F1} MB ({2}%)     " -f $receivedMB, $totalMB, $percent) -NoNewline -ForegroundColor Cyan
                }
                
                $lastUpdate = $now
                $lastBytes = $totalRead
            }
        }
        
        $fileStream.Close()
        $responseStream.Close()
        $response.Close()
        
        Write-Host ""
        $elapsed = (Get-Date) - $startTime
        Write-Host "  [OK] Download complete in $([math]::Round($elapsed.TotalSeconds, 1))s" -ForegroundColor Green
    }    
    catch {
        if ($fileStream) { $fileStream.Close() }
        if ($responseStream) { $responseStream.Close() }
        if ($response) { $response.Close() }
        
        Write-Host ""
        Write-Host "  [WARN] Stream download failed, falling back to Invoke-WebRequest..." -ForegroundColor Yellow
        
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $Url -OutFile $OutFile -UseBasicParsing
        $ProgressPreference = 'Continue'
    }
    
    if (-not (Test-Path $OutFile)) {
        throw "Download failed - file not found at: $OutFile"
    }
    
    $fileSize = (Get-Item $OutFile).Length
    if ($fileSize -eq 0) {
        Remove-Item $OutFile -Force
        throw "Download failed - file is empty (0 bytes)"
    }
    
    $fileSizeMB = [math]::Round($fileSize / 1MB, 2)
    Write-Host "  [OK] Verified: $fileSizeMB MB" -ForegroundColor Green
}

# ===========================
# INSTALLATION HANDLERS
# ===========================

function Setup-And-Install-Vcpkg {
    param([hashtable]$VcpkgPackages)

    if (-not $VcpkgPackages -or $VcpkgPackages.Count -eq 0) { return }

    Write-Host "`n=== vcpkg Setup & Packages ===" -ForegroundColor Magenta

    $vcpkgRoot = $env:VCPKG_ROOT
    $defaultRoot = 'C:\vcpkg'

    # Install vcpkg if missing
    if (-not $vcpkgRoot -or -not (Test-Path $vcpkgRoot)) {
        $vcpkgRoot = $defaultRoot
        Write-Host "  vcpkg not found. Installing to $vcpkgRoot..." -ForegroundColor Yellow

        if (-not (Test-Path $vcpkgRoot)) {
            git clone https://github.com/microsoft/vcpkg.git $vcpkgRoot
            if ($LASTEXITCODE -ne 0) { throw "Failed to clone vcpkg repository" }
        }

        Push-Location $vcpkgRoot
        try {
            Write-Host "  Bootstrapping vcpkg..." -ForegroundColor Yellow
            .\bootstrap-vcpkg.bat
            if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed" }
        }
        finally {
            Pop-Location
        }

        [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgRoot, "Machine")
        $env:VCPKG_ROOT = $vcpkgRoot
        Write-Host "  [OK] VCPKG_ROOT set to $vcpkgRoot" -ForegroundColor Green
    }
    else {
        Write-Host "  Using existing VCPKG_ROOT: $vcpkgRoot" -ForegroundColor Green
    }

    # Upgrade vcpkg and install packages
    Push-Location $vcpkgRoot
    try {
        Write-Host "  Upgrading vcpkg (git pull + bootstrap)..." -ForegroundColor Yellow
        git pull origin master --quiet
        .\bootstrap-vcpkg.bat

        $vcpkgExe = if (Test-Path "vcpkg.exe") { "vcpkg.exe" } else { "vcpkg" }
        $triplet = "x64-windows"

        foreach ($entry in $VcpkgPackages.GetEnumerator()) {
            $port = $entry.Key
            Write-Host "`n[$port] Installing $port`:$triplet ..." -ForegroundColor Cyan
            & $vcpkgExe install "$port`:$triplet"
            if ($LASTEXITCODE -ne 0) {
                throw "vcpkg install failed for port: $port"
            }
            Write-Host "  [OK] $port installed" -ForegroundColor Green
        }
    }
    finally {
        Pop-Location
    }

    # Add standard vcpkg paths to system environment
    $instDir = Join-Path $vcpkgRoot "installed\$triplet"
    if (Test-Path $instDir) {
        $inc = Join-Path $instDir "include"
        if (Test-Path $inc) { Add-ToSystemInclude $inc }

        $lib = Join-Path $instDir "lib"
        if (Test-Path $lib) { Add-ToSystemLib $lib }

        $bin = Join-Path $instDir "bin"
        if (Test-Path $bin) { Add-ToSystemPath $bin }

        $dbgLib = Join-Path $instDir "debug\lib"
        if (Test-Path $dbgLib) { Add-ToSystemLib $dbgLib }

        $dbgBin = Join-Path $instDir "debug\bin"
        if (Test-Path $dbgBin) { Add-ToSystemPath $dbgBin }

        Write-Host "  [OK] vcpkg paths added to INCLUDE/LIB/PATH" -ForegroundColor Green
    }

    # Set CMAKE_TOOLCHAIN_FILE (recommended and sufficient for local + CI)
    $toolchainFile = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"
    if (Test-Path $toolchainFile) {
        # Convert backslashes to forward slashes so CMake doesn't treat them as escape sequences
        $toolchainFile = $toolchainFile -replace '\\', '/'
        
        [Environment]::SetEnvironmentVariable("CMAKE_TOOLCHAIN_FILE", $toolchainFile, "Machine")
        $env:CMAKE_TOOLCHAIN_FILE = $toolchainFile
        Write-Host "  [OK] CMAKE_TOOLCHAIN_FILE set to: $toolchainFile" -ForegroundColor Green
    }
    else {
        Write-Warning "vcpkg toolchain file not found at expected location"
    }
}

function Install-SystemTool($name, $config) {
    Write-Host "`n[$name] Checking system tool..." -ForegroundColor Cyan

    $isInstalled = if ($config.Verify -like '*\*' -or $config.Verify -like '*:*') {
        Test-Path $config.Verify
    } else {
        Test-Command $config.Verify
    }

    if ($isInstalled) {
        Write-Host "  [OK] Already installed" -ForegroundColor Green
        return
    }

    Write-Host "  Installing via winget..." -ForegroundColor Yellow
    winget install --id $config.WingetId -e --accept-package-agreements --accept-source-agreements --silent

    $isInstalled = if ($config.Verify -like '*\*' -or $config.Verify -like '*:*') {
        Test-Path $config.Verify
    } else {
        Test-Command $config.Verify
    }

    if (-not $isInstalled) {
        throw "Installation verification failed for $name"
    }
    Write-Host "  [OK] Installed successfully" -ForegroundColor Green
}

function Install-BinaryPackage($name, $config) {
    Write-Host "`n[$name] Installing binary package..." -ForegroundColor Cyan

    $effectiveRoot = if ($config.Version) {
        Join-Path (Split-Path $config.InstallRoot -Parent) $config.Version
    } else {
        $config.InstallRoot
    }

    if ($config.Version) {
        $config.InstallRoot = $effectiveRoot
        if ($config.Verify -and $config.Verify -like "*LLVM_Libs\*") {
            $config.Verify = $config.Verify -replace 'LLVM_Libs\\[^\\]+', "LLVM_Libs\$($config.Version)"
        }
    }

    if (Test-Path $config.Verify) {
        Write-Host "  [OK] Already installed at $($config.InstallRoot)" -ForegroundColor Green
        return $config.InstallRoot
    }

    $tempDir = Join-Path $env:TEMP "MayaFlux-setup\$name"
    $downloadFile = Join-Path $tempDir (Split-Path $config.Url -Leaf)

    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
    New-Item -ItemType Directory -Path $config.InstallRoot -Force | Out-Null

    if (-not (Test-Path $downloadFile)) {
        Write-Host "  Downloading from $($config.Url)..." -ForegroundColor Yellow
        Invoke-WithRetry {
            Invoke-FastDownload -Url $config.Url -OutFile $downloadFile
        }
    } else {
        Write-Host "  [OK] Using cached download: $downloadFile" -ForegroundColor Green
    }

    if (-not (Test-Path $downloadFile)) {
        throw "Download file missing: $downloadFile"
    }

    $fileSize = [math]::Round((Get-Item $downloadFile).Length / 1MB, 2)
    if ($fileSize -eq 0) {
        Remove-Item $downloadFile -Force -ErrorAction SilentlyContinue
        throw "Downloaded file is empty (0 bytes): $downloadFile"
    }

    Write-Host "  [OK] Download verified: $fileSize MB at $downloadFile" -ForegroundColor Green

    if ($downloadFile -match "\.exe$" -and $config.InstallArgs) {
        Write-Host "  Running installer..." -ForegroundColor Yellow
        
        if ($name -eq "VulkanSDK") {
            Write-Host ""
            Write-Host "  ============================================" -ForegroundColor Cyan
            Write-Host "  Vulkan SDK Installer" -ForegroundColor Cyan
            Write-Host "  ============================================" -ForegroundColor Cyan
            Write-Host "  The Vulkan SDK installer will now open." -ForegroundColor Yellow
            Write-Host "  " -ForegroundColor Yellow
            Write-Host "  IMPORTANT: Install ALL components" -ForegroundColor Yellow
            Write-Host "  (SDL2 is optional - install if needed)" -ForegroundColor Yellow
            Write-Host "  (ARM components are not needed)" -ForegroundColor Yellow
            Write-Host "  " -ForegroundColor Yellow
            Write-Host "  The installer will run, please complete it." -ForegroundColor Yellow
            Write-Host "  ============================================" -ForegroundColor Cyan
            Write-Host ""
            Start-Sleep -Seconds 3
        }
        
        $installArgs = @()
        $installArgs += $config.InstallArgs
        $installArgs += "/D=$($config.InstallRoot)"
        
        Start-Process -FilePath $downloadFile -ArgumentList $installArgs -Wait
    }    
    else {
        # Extract archive
        Write-Host "  Extracting..." -ForegroundColor Yellow
        $extractDir = Join-Path $env:TEMP "MayaFlux-extract\$name-$(Get-Random)"
        
        Expand-ArchiveSafe -archivePath $downloadFile -destination $extractDir

        # Remove the archive file to avoid interfering with content detection
        Remove-Item $downloadFile -Force -ErrorAction SilentlyContinue

        # === INTELLIGENT CONTENT MOVE (our wrapper handling) ===
        Write-Host "  Organizing extracted files..." -ForegroundColor Yellow

        $extractedItems = Get-ChildItem $extractDir -Force | Where-Object { -not $_.Name.StartsWith('.') }
        $extractedDirs  = $extractedItems | Where-Object { $_.PSIsContainer }

        if ($extractedDirs.Count -eq 1 -and $extractedItems.Count -eq 1) {
            # Single wrapper directory case (GLFW, FFmpeg, LLVM, etc.)
            $wrapperDir = $extractedDirs[0].FullName
            Write-Host "  Stripping single wrapper directory: $($extractedDirs[0].Name)" -ForegroundColor Cyan
            Get-ChildItem $wrapperDir -Force | Move-Item -Destination $config.InstallRoot -Force
        }
        else {
            # Multiple top-level items (HIDAPI) or flat structure
            Write-Host "  Moving all top-level items directly" -ForegroundColor Cyan
            Get-ChildItem $extractDir -Force | Move-Item -Destination $config.InstallRoot -Force
        }

        # Cleanup extraction directory
        Remove-Item $extractDir -Recurse -Force -ErrorAction SilentlyContinue
    }

    # Cleanup
    Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue

    if (-not (Test-Path $config.Verify)) {
        throw "Installation verification failed for $name"
    }

    Write-Host "  [OK] Installed to $($config.InstallRoot)" -ForegroundColor Green
    return $config.InstallRoot
}

function Install-HeaderOnlyPackage($name, $config) {
    Write-Host "`n[$name] Installing header-only package..." -ForegroundColor Cyan
    
    if (Test-Path $config.Verify) {
        Write-Host "  [OK] Already installed at $($config.InstallRoot)" -ForegroundColor Green
        return
    }

    $tempDir = Join-Path $env:TEMP "MayaFlux-setup\$name"
    $downloadFile = Join-Path $tempDir (Split-Path $config.Url -Leaf)

    # Create target directory
    $targetIncludeDir = Join-Path $config.InstallRoot "include"
    New-Item -ItemType Directory -Path $targetIncludeDir -Force | Out-Null
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    # Download and extract
    if (-not (Test-Path $downloadFile)) {
        Write-Host "  Downloading..." -ForegroundColor Yellow
        Invoke-WebRequest -Uri $config.Url -OutFile $downloadFile -UseBasicParsing
    }

    Write-Host "  Extracting..." -ForegroundColor Yellow
    Expand-ArchiveSafe $downloadFile $tempDir

    # Find extracted directory (skip hidden directories like .git)
    $extracted = Get-ChildItem $tempDir -Directory | Where-Object { -not $_.Name.StartsWith('.') }
    
    # If there's exactly one non-hidden directory, use it as the base
    # Otherwise, use tempDir itself (multiple directories = no wrapper)
    if ($extracted.Count -eq 1) {
        $baseDir = $extracted[0].FullName
    } else {
        $baseDir = $tempDir
    }

    # Copy headers based on config
    if ($config.HeaderSubpath) {
        $srcDir = Join-Path $baseDir $config.HeaderSubpath
        $targetDir = if ($config.TargetSubdir) { 
            Join-Path $targetIncludeDir $config.TargetSubdir 
        } else { 
            $targetIncludeDir 
        }
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        Copy-Item -Path "$srcDir\*" -Destination $targetDir -Recurse -Force
    }
    elseif ($config.HeaderPattern) {
        $targetDir = if ($config.TargetSubdir) { 
            Join-Path $targetIncludeDir $config.TargetSubdir 
        } else { 
            $targetIncludeDir 
        }
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        Get-ChildItem $baseDir -Filter $config.HeaderPattern | Copy-Item -Destination $targetDir -Force
    }
    else {
        Copy-Item -Path "$baseDir\*" -Destination $config.InstallRoot -Recurse -Force
    }

    Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue

    if (-not (Test-Path $config.Verify)) {
        throw "Installation verification failed for $name"
    }

    Write-Host "  [OK] Headers installed to $targetIncludeDir" -ForegroundColor Green
}

function Install-SourcePackage($name, $config) {
    Write-Host "`n[$name] Building from source..." -ForegroundColor Cyan

    if (Test-Path $config.Verify) {
        Write-Host "  [OK] Already installed at $($config.InstallRoot)" -ForegroundColor Green
        return
    }

    $srcDir = Join-Path $env:TEMP "MayaFlux-build\$name-src"
    $buildDir = Join-Path $env:TEMP "MayaFlux-build\$name-build"

    # Clean previous attempts
    Remove-Item $srcDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $buildDir -Recurse -Force -ErrorAction SilentlyContinue

    # Get source
    if ($config.GitUrl) {
        Write-Host "  Cloning repository..." -ForegroundColor Yellow
        git clone --depth 1 --branch $config.GitBranch $config.GitUrl $srcDir
        if ($LASTEXITCODE -ne 0) {
            throw "Git clone failed for $name"
        }
    }
    else {
        Write-Host "  Downloading source..." -ForegroundColor Yellow
        $archive = Join-Path $env:TEMP "$name.tar.xz"
        Invoke-WebRequest -Uri $config.Url -OutFile $archive -UseBasicParsing
        tar -xf $archive -C $env:TEMP
        $extracted = Get-ChildItem $env:TEMP -Directory | Where-Object { $_.Name -like "$name*" } | Select-Object -First 1
        Move-Item $extracted.FullName $srcDir
        Remove-Item $archive
    }

    # Configure CMake
    Write-Host "  Configuring..." -ForegroundColor Yellow
    $cmakeArgs = @(
        "-S", $srcDir,
        "-B", $buildDir,
        "-DCMAKE_INSTALL_PREFIX=$($config.InstallRoot)"
    )

    if ($config.Generator) {
        $cmakeArgs += @("-G", $config.Generator)
    }

    foreach ($opt in $config.CMakeOptions.GetEnumerator()) {
        $cmakeArgs += "-D$($opt.Key)=$($opt.Value)"
    }

    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed for $name"
    }

    # Build and install
    Write-Host "  Building..." -ForegroundColor Yellow
    cmake --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for $name"
    }
    
    Write-Host "  Installing..." -ForegroundColor Yellow
    cmake --install $buildDir --config Release
    if ($LASTEXITCODE -ne 0) {
        throw "Install failed for $name"
    }

    # Cleanup
    Remove-Item $srcDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $buildDir -Recurse -Force -ErrorAction SilentlyContinue

    if (-not (Test-Path $config.Verify)) {
        throw "Installation verification failed for $name"
    }

    Write-Host "  [OK] Built and installed to $($config.InstallRoot)" -ForegroundColor Green
}

# ===========================
# ENVIRONMENT SETUP
# ===========================

function Set-PackageEnvironment($name, $config, $installedPath = $null) {
    if ($SkipEnvSetup) { return }

    # Set environment variables
    if ($config.EnvVars) {
        foreach ($var in $config.EnvVars.GetEnumerator()) {
            $value = if ($var.Value -is [scriptblock]) {
                & $var.Value
            }
            else {
                $var.Value
            }

            if ($value) {
                [Environment]::SetEnvironmentVariable($var.Key, $value, "Machine")
                Set-Item -Path "env:$($var.Key)" -Value $value
            }
        }
    }

    # Special handling for VulkanSDK - set version-specific env vars
    if ($name -eq "VulkanSDK") {
        $rootPath = if ($installedPath) { $installedPath } else { $config.InstallRoot }
        # Find the actual version directory
        $versionDir = Get-ChildItem $rootPath -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($versionDir) {
            $vulkanSdkFull = $versionDir.FullName
            [Environment]::SetEnvironmentVariable("VULKAN_SDK", $vulkanSdkFull, "Machine")
            [Environment]::SetEnvironmentVariable("VK_SDK_PATH", $vulkanSdkFull, "Machine")
            Write-Host "  + Set VULKAN_SDK=$vulkanSdkFull" -ForegroundColor Green
            Write-Host "  + Set VK_SDK_PATH=$vulkanSdkFull" -ForegroundColor Green
            
            $binPath = Join-Path $vulkanSdkFull "Bin"
            if (Test-Path $binPath) {
                Add-ToSystemPath $binPath
            }
        }
    }

    $rootPath = if ($installedPath) { $installedPath } else { $config.InstallRoot }
    if ($config.PathAdditions) {
        # Evaluate scriptblock if needed
        $paths = if ($config.PathAdditions -is [scriptblock]) {
            $evaluated = & $config.PathAdditions
            # Handle case where scriptblock returns an array
            if ($evaluated -is [array]) { $evaluated } else { @($evaluated) }
        }
        else {
            $config.PathAdditions
        }

        foreach ($subpath in $paths) {
            if (-not $subpath) { continue }

            $fullPath = if ([System.IO.Path]::IsPathRooted($subpath)) {
                $subpath
            }
            else {
                Join-Path $rootPath $subpath
            }

            if (Test-Path $fullPath) {
                Add-ToSystemPath $fullPath
            }
        }
    }

    # Add to LIB
    if ($config.LibAdditions) {
        foreach ($subpath in $config.LibAdditions) {
            $fullPath = Join-Path $rootPath $subpath
            if (Test-Path $fullPath) {
                Add-ToSystemLib $fullPath
            }
        }
    }

    # Add to INCLUDE
    if ($config.IncludeAdditions) {
        foreach ($subpath in $config.IncludeAdditions) {
            $fullPath = Join-Path $rootPath $subpath
            if (Test-Path $fullPath) {
                Add-ToSystemInclude $fullPath
            }
        }
    }
}

# ===========================
# MAIN INSTALLATION FLOW
# ===========================

Write-Host "=== MayaFlux Package Installer ===" -ForegroundColor Cyan
Write-Host ""

# Load package definitions
$packages = Import-PowerShellDataFile $PackagesFile

# Check prerequisites
Write-Host "Checking prerequisites..." -ForegroundColor Cyan
if (-not (Test-Command "winget")) {
    Write-Error "Winget is required. Please install App Installer from Microsoft Store."
    exit 1
}

# Install system tools first
Write-Host "`n=== System Tools ===" -ForegroundColor Magenta
foreach ($tool in $packages.SystemTools.GetEnumerator()) {
    if ($Only -and $tool.Key -notin $Only) { continue }
    Install-SystemTool $tool.Key $tool.Value
}

# Check special packages
Write-Host "`n=== Special Requirements ===" -ForegroundColor Magenta
foreach ($special in $packages.SpecialPackages.GetEnumerator()) {
    if (-not (Test-Path $special.Value.Verify)) {
        Write-Warning "$($special.Key): $($special.Value.Message)"
    }
    else {
        Write-Host "[$($special.Key)] [OK] Found" -ForegroundColor Green
        Set-PackageEnvironment $special.Key $special.Value
    }
}

# Install binary packages
Write-Host "`n=== Binary Packages ===" -ForegroundColor Magenta
foreach ($pkg in $packages.BinaryPackages.GetEnumerator()) {
    if ($Only -and $pkg.Key -notin $Only) { continue }
    $installedPath = Install-BinaryPackage $pkg.Key $pkg.Value
    Set-PackageEnvironment $pkg.Key $pkg.Value $installedPath
}

# Install Winget packages
Write-Host "`n=== Winget Packages ===" -ForegroundColor Magenta
foreach ($pkg in $packages.WingetPackages.GetEnumerator()) {
    if ($Only -and $pkg.Key -notin $Only) { continue }
    Install-SystemTool $pkg.Key $pkg.Value
}

# Setup vcpkg + install packages
Write-Host "`n=== Vcpkg Packages ===" -ForegroundColor Magenta
Setup-And-Install-Vcpkg -VcpkgPackages $packages.VcpkgPackages

# Install header-only packages
Write-Host "`n=== Header-Only Packages ===" -ForegroundColor Magenta
foreach ($pkg in $packages.HeaderOnlyPackages.GetEnumerator()) {
    if ($Only -and $pkg.Key -notin $Only) { continue }
    Install-HeaderOnlyPackage $pkg.Key $pkg.Value
    Set-PackageEnvironment $pkg.Key $pkg.Value
}

# Build source packages
Write-Host "`n=== Source Packages ===" -ForegroundColor Magenta
foreach ($pkg in $packages.SourcePackages.GetEnumerator()) {
    if ($Only -and $pkg.Key -notin $Only) { continue }

    # Skip LibXml2 build if headers-only
    if ($pkg.Value.HeadersOnly) {
        Install-HeaderOnlyPackage $pkg.Key $pkg.Value
    }
    else {
        Install-SourcePackage $pkg.Key $pkg.Value
    }
    Set-PackageEnvironment $pkg.Key $pkg.Value
}

# Setup MayaFlux structure
Write-Host "`n=== MayaFlux Installation ===" -ForegroundColor Magenta
$mfConfig = $packages.MayaFlux
if (-not (Test-Path $mfConfig.InstallRoot)) {
    Write-Host "Creating MayaFlux directory structure..." -ForegroundColor Yellow
    foreach ($subdir in $mfConfig.Subdirectories) {
        $path = Join-Path $mfConfig.InstallRoot $subdir
        New-Item -ItemType Directory -Path $path -Force | Out-Null
    }
}
Set-PackageEnvironment "MayaFlux" $mfConfig

Write-Host "`n=== Installation Complete ===" -ForegroundColor Green
Write-Host "Restart your terminal to use updated environment variables." -ForegroundColor Yellow

# Ensure clean exit
exit 0
