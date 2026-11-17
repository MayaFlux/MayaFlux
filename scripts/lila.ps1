<#
.SYNOPSIS
Interactive client for a running Lila session, over a Unix socket or TCP.

.DESCRIPTION
Keeps one connection open and sends each entered line as a message. Every
message is answered with one line, empty on success or a JSON error. Windows
ncat has no Unix socket support, which is why this exists.

Entering {{ starts a block, and every following line up to }} is sent as one
message. Entering :q ends the session. The connection is reopened on the next
message if the session restarted.

.PARAMETER Path
Socket file. Defaults to mayaflux-lila.sock in the runtime directory.

.PARAMETER Tcp
host:port of a TCP listener, for example 127.0.0.1:9090. Overrides Path.

.PARAMETER Send
Sends this text as one message and exits.

.NOTES
Needs PowerShell 7 or later. If script execution is restricted, run it with
pwsh -ExecutionPolicy Bypass -File lila.ps1.

.EXAMPLE
lila.ps1

.EXAMPLE
lila.ps1 -Tcp 127.0.0.1:9090

.EXAMPLE
lila.ps1 -Send 'auto f = MayaFlux::live_cast<Sine>("Sine_0"); f->set_frequency(100);'
#>
#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Path = (Join-Path $(if ($env:XDG_RUNTIME_DIR) { $env:XDG_RUNTIME_DIR } elseif ($env:TEMP) { $env:TEMP } else { $env:TMP }) 'mayaflux-lila.sock'),
    [string]$Tcp,
    [string]$Send
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Connect-Lila {
    if ($Tcp) {
        $host_name, $port = $Tcp -split ':'
        $socket = [System.Net.Sockets.Socket]::new('InterNetwork', 'Stream', 'Tcp')
        $socket.Connect($host_name, [int]$port)
    } else {
        $socket = [System.Net.Sockets.Socket]::new('Unix', 'Stream', 'Unspecified')
        try {
            $socket.Connect([System.Net.Sockets.UnixDomainSocketEndPoint]::new($Path))
        } catch {
            $socket.Dispose()
            throw
        }
    }
    $socket.ReceiveTimeout = 120000
    return $socket
}

function Send-Lila {
    param([System.Net.Sockets.Socket]$Socket, [string]$Text)
    $bytes = [Text.Encoding]::UTF8.GetBytes($Text.TrimEnd("`r", "`n") + "`n")
    [void]$Socket.Send($bytes)
    $buffer = New-Object byte[] 65536
    $count = $Socket.Receive($buffer)
    if ($count -eq 0) {
        throw [System.IO.IOException]::new('the session closed the connection')
    }
    return [Text.Encoding]::UTF8.GetString($buffer, 0, $count).TrimEnd()
}

function Invoke-Lila {
    param([string]$Text)
    try {
        if (-not $script:socket) {
            $script:socket = Connect-Lila
        }
        $reply = Send-Lila -Socket $script:socket -Text $Text
        if ($reply) { Write-Host $reply }
        return $true
    } catch {
        if ($script:socket) {
            $script:socket.Dispose()
            $script:socket = $null
        }
        Write-Host "error: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

$script:socket = $null
$target = if ($Tcp) { $Tcp } else { $Path }

try {
    if ($Send) {
        if (-not (Invoke-Lila -Text $Send)) {
            exit 1
        }
        return
    }

    Write-Host "lila: $target  ({{ ... }} for a block, :q to quit)"
    while ($true) {
        $line = Read-Host 'lila'
        if ($null -eq $line -or $line -eq ':q') {
            break
        }
        if ($line -eq '{{') {
            $block = @()
            while (($next = Read-Host '  ...') -ne '}}') {
                $block += $next
            }
            $line = $block -join "`n"
        }
        if (-not $line.Trim()) {
            continue
        }
        [void](Invoke-Lila -Text $line)
    }
} finally {
    if ($script:socket) {
        $script:socket.Dispose()
    }
}
