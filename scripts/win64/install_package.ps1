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
    [bool](Get-Command $cmd -ErrorAction SilentlyContinue)
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
    $ext = [System.IO.Path]::GetExtension($archivePath).ToLower()

    switch ($ext) {
        ".zip" {
            Expand-Archive -Path $archivePath -DestinationPath $destination -Force
        }
        { $_ -in ".tar", ".xz", ".gz" } {
            # Use native tar for tar/xz/gz
            tar -xf $archivePath -C $destination
            if ($LASTEXITCODE -ne 0) { throw "tar extraction failed" }
        }
        ".7z" {
            $sevenZip = "${env:ProgramFiles}\7-Zip\7z.exe"
            if (-not (Test-Path $sevenZip)) { throw "7-Zip required for .7z files" }
            & $sevenZip x $archivePath "-o$destination" -y | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "7-Zip extraction failed" }
        }
        default { throw "Unsupported archive format: $ext" }
    }
}

# ===========================
# INSTALLATION HANDLERS
# ===========================

function Install-SystemTool($name, $config) {
    Write-Host "`n[$name] Checking system tool..." -ForegroundColor Cyan

    if (& $config.Verify) {
        Write-Host "  [OK] Already installed" -ForegroundColor Green
        return
    }

    Write-Host "  Installing via winget..." -ForegroundColor Yellow
    winget install --id $config.WingetId -e --accept-package-agreements --accept-source-agreements --silent

    if (-not (& $config.Verify)) {
        throw "Installation verification failed for $name"
    }
    Write-Host "  [OK] Installed successfully" -ForegroundColor Green
}

function Install-BinaryPackage($name, $config) {
    Write-Host "`n[$name] Installing binary package..." -ForegroundColor Cyan

    if (& $config.Verify) {
        Write-Host "  [OK] Already installed at $($config.InstallRoot)" -ForegroundColor Green
        return $config.InstallRoot
    }

    $tempDir = Join-Path $env:TEMP "MayaFlux-setup\$name"
    $downloadFile = Join-Path $tempDir (Split-Path $config.Url -Leaf)

    # Create directories
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
    New-Item -ItemType Directory -Path $config.InstallRoot -Force | Out-Null

    # Download
    if (-not (Test-Path $downloadFile)) {
        Write-Host "  Downloading from $($config.Url)..." -ForegroundColor Yellow
        Invoke-WithRetry {
            Invoke-WebRequest -Uri $config.Url -OutFile $downloadFile -UseBasicParsing
        }
    }

    # Handle installer vs archive
    if ($downloadFile -match "\.exe$" -and $config.InstallArgs) {
        Write-Host "  Running installer..." -ForegroundColor Yellow
        $installArgs = $config.InstallArgs + @("/D=$($config.InstallRoot)")
        Start-Process -FilePath $downloadFile -ArgumentList $installArgs -Wait
    }
    else {
        # Extract archive
        Write-Host "  Extracting..." -ForegroundColor Yellow
        Expand-ArchiveSafe $downloadFile $tempDir

        # Move contents to install root
        $extracted = Get-ChildItem $tempDir -Directory | Where-Object { $_.Name -ne (Split-Path $tempDir -Leaf) } | Select-Object -First 1
        if ($extracted) {
            Get-ChildItem $extracted.FullName | Move-Item -Destination $config.InstallRoot -Force
        }
    }

    # Cleanup
    Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue

    if (-not (& $config.Verify)) {
        throw "Installation verification failed for $name"
    }

    Write-Host "  [OK] Installed to $($config.InstallRoot)" -ForegroundColor Green
    return $config.InstallRoot
}

function Install-HeaderOnlyPackage($name, $config) {
    Write-Host "`n[$name] Installing header-only package..." -ForegroundColor Cyan

    if (& $config.Verify) {
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

    # Find extracted directory
    $extracted = Get-ChildItem $tempDir -Directory | Select-Object -First 1

    # Copy headers based on config
    if ($config.HeaderSubpath) {
        $srcDir = Join-Path $extracted.FullName $config.HeaderSubpath
        $targetDir = if ($config.TargetSubdir) { Join-Path $targetIncludeDir $config.TargetSubdir } else { $targetIncludeDir }
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        Copy-Item -Path "$srcDir\*" -Destination $targetDir -Recurse -Force
    }
    elseif ($config.HeaderPattern) {
        $targetDir = if ($config.TargetSubdir) { Join-Path $targetIncludeDir $config.TargetSubdir } else { $targetIncludeDir }
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        Get-ChildItem $extracted.FullName -Filter $config.HeaderPattern | Copy-Item -Destination $targetDir -Force
    }
    else {
        Copy-Item -Path "$($extracted.FullName)\*" -Destination $config.InstallRoot -Recurse -Force
    }

    Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue

    if (-not (& $config.Verify)) {
        throw "Installation verification failed for $name"
    }

    Write-Host "  [OK] Headers installed to $targetIncludeDir" -ForegroundColor Green
}

function Install-SourcePackage($name, $config) {
    Write-Host "`n[$name] Building from source..." -ForegroundColor Cyan

    if (& $config.Verify) {
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
        git clone --depth 1 --branch $config.GitBranch --quiet $config.GitUrl $srcDir 2>$null
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

    & cmake @cmakeArgs 2>$null | Out-Null

    # Build and install
    Write-Host "  Building..." -ForegroundColor Yellow
    cmake --build $buildDir --config Release 2>$null | Out-Null
    cmake --install $buildDir --config Release 2>$null | Out-Null

    # Cleanup
    Remove-Item $srcDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $buildDir -Recurse -Force -ErrorAction SilentlyContinue

    if (-not (& $config.Verify)) {
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
    if (-not (& $special.Value.Verify)) {
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
