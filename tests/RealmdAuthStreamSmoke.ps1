param(
    [Parameter(Mandatory = $true)]
    [string]$RealmdPath,

    [Parameter(Mandatory = $true)]
    [string]$ConfigPath,

    [int]$Port = 43724,
    [int]$TimeoutSeconds = 2
)

$ErrorActionPreference = "Stop"

function Set-ConfigValue {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [AllowEmptyCollection()]
        [string[]]$Lines,

        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $pattern = "^\s*" + [regex]::Escape($Name) + "\s*="
    $found = $false
    $updated = foreach ($line in $Lines) {
        if ($line -match $pattern) {
            $found = $true
            "$Name = $Value"
        }
        else {
            $line
        }
    }

    if (-not $found) {
        $updated += "$Name = $Value"
    }

    return $updated
}

function Wait-ForListener {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ListenerPort,

        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process
    )

    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($Process.HasExited) {
            throw "realmd exited before opening the smoke-test listener (exit code $($Process.ExitCode))."
        }

        $client = [System.Net.Sockets.TcpClient]::new()
        try {
            $connect = $client.ConnectAsync("127.0.0.1", $ListenerPort)
            if ($connect.Wait(200) -and $client.Connected) {
                return
            }
        }
        catch {
            Write-Verbose "realmd listener not ready yet: $($_.Exception.Message)"
        }
        finally {
            $client.Dispose()
        }

        Start-Sleep -Milliseconds 100
    }

    throw "realmd did not open 127.0.0.1:$ListenerPort within 15 seconds."
}

function New-Client {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ListenerPort
    )

    $client = [System.Net.Sockets.TcpClient]::new()
    $client.NoDelay = $true
    $client.Connect("127.0.0.1", $ListenerPort)
    $client.ReceiveTimeout = $TimeoutSeconds * 1000
    return $client
}

function Test-PromptClose {
    param(
        [Parameter(Mandatory = $true)]
        [System.Net.Sockets.TcpClient]$Client
    )

    if (-not $Client.Connected) {
        return $true
    }

    $stream = $Client.GetStream()
    $buffer = [byte[]]::new(1)
    try {
        return $stream.Read($buffer, 0, 1) -eq 0
    }
    catch [System.IO.IOException] {
        return $false
    }
}

function Read-Response {
    param(
        [Parameter(Mandatory = $true)]
        [System.Net.Sockets.TcpClient]$Client
    )

    $stream = $Client.GetStream()
    $buffer = [byte[]]::new(512)
    $bytes = [System.Collections.Generic.List[byte]]::new()

    try {
        $read = $stream.Read($buffer, 0, $buffer.Length)
        if ($read -eq 0) {
            return [byte[]]::new(0)
        }

        $bytes.AddRange([byte[]]$buffer[0..($read - 1)])
        $stream.ReadTimeout = 150

        while ($true) {
            try {
                $read = $stream.Read($buffer, 0, $buffer.Length)
                if ($read -eq 0) {
                    break
                }
                $bytes.AddRange([byte[]]$buffer[0..($read - 1)])
            }
            catch [System.IO.IOException] {
                break
            }
        }
    }
    catch [System.IO.IOException] {
        return [byte[]]::new(0)
    }

    return $bytes.ToArray()
}

function New-LogonChallenge {
    $username = [System.Text.Encoding]::ASCII.GetBytes("NOUSER")
    $bodySize = 30 + $username.Length
    $packet = [System.Collections.Generic.List[byte]]::new()

    $packet.Add(0x00)
    $packet.Add(0x00)
    $packet.Add([byte]($bodySize -band 0xff))
    $packet.Add([byte](($bodySize -shr 8) -band 0xff))
    $packet.AddRange([System.Text.Encoding]::ASCII.GetBytes("WoW`0"))
    $packet.AddRange([byte[]](1, 12, 1))
    $packet.Add(0xf3)
    $packet.Add(0x16)
    $packet.AddRange([System.Text.Encoding]::ASCII.GetBytes("x86`0"))
    $packet.AddRange([System.Text.Encoding]::ASCII.GetBytes("Win`0"))
    $packet.AddRange([System.Text.Encoding]::ASCII.GetBytes("enUS"))
    $packet.AddRange([byte[]](0, 0, 0, 0))
    $packet.AddRange([byte[]](1, 0, 0, 127))
    $packet.Add([byte]$username.Length)
    $packet.AddRange($username)

    return $packet.ToArray()
}

function Invoke-Challenge {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ListenerPort,

        [Parameter(Mandatory = $true)]
        [byte[]]$Packet,

        [int]$SplitAt = 0
    )

    $client = New-Client -ListenerPort $ListenerPort
    try {
        $stream = $client.GetStream()
        if ($SplitAt -gt 0) {
            $stream.Write($Packet, 0, $SplitAt)
            $stream.Flush()
            Start-Sleep -Milliseconds 200
            $stream.Write($Packet, $SplitAt, $Packet.Length - $SplitAt)
        }
        else {
            $stream.Write($Packet, 0, $Packet.Length)
        }
        $stream.Flush()
        return Read-Response -Client $client
    }
    finally {
        $client.Dispose()
    }
}

$realmd = (Resolve-Path -LiteralPath $RealmdPath).Path
$sourceConfig = (Resolve-Path -LiteralPath $ConfigPath).Path
$tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$testRoot = Join-Path $tempRoot ("realmd-auth-stream-smoke-" + [guid]::NewGuid().ToString("N"))
$process = $null
$failures = [System.Collections.Generic.List[string]]::new()

try {
    New-Item -ItemType Directory -Path $testRoot | Out-Null
    $testConfig = Join-Path $testRoot "realmd-smoke.conf"
    $stdoutPath = Join-Path $testRoot "realmd.stdout.log"
    $stderrPath = Join-Path $testRoot "realmd.stderr.log"
    $logPath = Join-Path $testRoot "realmd-smoke.log"

    $config = Get-Content -LiteralPath $sourceConfig
    $config = Set-ConfigValue -Lines $config -Name "BindIP" -Value '"127.0.0.1"'
    $config = Set-ConfigValue -Lines $config -Name "RealmServerPort" -Value $Port
    $config = Set-ConfigValue -Lines $config -Name "AuthSessionTimeout" -Value $TimeoutSeconds
    $config = Set-ConfigValue -Lines $config -Name "LogsDir" -Value ('"' + ($testRoot -replace '\\', '/') + '/"')
    $config = Set-ConfigValue -Lines $config -Name "PidFile" -Value '""'
    $config = Set-ConfigValue -Lines $config -Name "ScheduledExit.Enable" -Value "0"
    $config = Set-ConfigValue -Lines $config -Name "LogFile" -Value '"realmd-smoke.log"'
    $config = Set-ConfigValue -Lines $config -Name "LogLevel" -Value "3"
    $config = Set-ConfigValue -Lines $config -Name "LogFileLevel" -Value "3"
    $config = Set-ConfigValue -Lines $config -Name "LogTimestamp" -Value "0"
    Set-Content -LiteralPath $testConfig -Value $config -Encoding ASCII

    $process = Start-Process -FilePath $realmd `
        -ArgumentList @("-c", $testConfig) `
        -WorkingDirectory (Split-Path -Parent $realmd) `
        -WindowStyle Hidden `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -PassThru

    Wait-ForListener -ListenerPort $Port -Process $process

    $invalidClient = New-Client -ListenerPort $Port
    try {
        $invalidStream = $invalidClient.GetStream()
        $invalidStream.WriteByte(0xff)
        $chunk = [byte[]]::new(4096)
        for ($index = 0; $index -lt 20; ++$index) {
            try {
                $invalidStream.Write($chunk, 0, $chunk.Length)
            }
            catch {
                break
            }
        }

        if (-not (Test-PromptClose -Client $invalidClient)) {
            $failures.Add("unknown command input was retained instead of closing promptly")
        }
    }
    finally {
        $invalidClient.Dispose()
    }

    $challenge = New-LogonChallenge
    [byte[]]$wholeResponse = @(Invoke-Challenge -ListenerPort $Port -Packet $challenge)
    if ($wholeResponse.Length -eq 0) {
        $failures.Add("complete build-5875 challenge produced no response")
    }

    $liveLogDeadline = [DateTime]::UtcNow.AddSeconds(3)
    $liveAuthLineVisible = $false
    while ([DateTime]::UtcNow -lt $liveLogDeadline) {
        if ((Test-Path -LiteralPath $logPath) -and
            (Select-String -LiteralPath $logPath `
                -SimpleMatch "[Auth] Received command 0" `
                -Quiet)) {
            $liveAuthLineVisible = $true
            break
        }
        Start-Sleep -Milliseconds 100
    }

    if (-not $liveAuthLineVisible) {
        $failures.Add(
            "debug auth records were not flushed to the live log within 3 seconds"
        )
    }

    for ($split = 1; $split -lt $challenge.Length; ++$split) {
        [byte[]]$fragmentedResponse = @(
            Invoke-Challenge -ListenerPort $Port -Packet $challenge -SplitAt $split
        )
        if (-not [System.Linq.Enumerable]::SequenceEqual(
                [byte[]]$wholeResponse,
                [byte[]]$fragmentedResponse)) {
            $failures.Add(
                "build-5875 challenge split at byte $split did not match the complete-packet response"
            )
            break
        }
    }

    $idleClient = New-Client -ListenerPort $Port
    try {
        $idleClient.ReceiveTimeout = ($TimeoutSeconds + 2) * 1000
        if (-not (Test-PromptClose -Client $idleClient)) {
            $failures.Add("idle unauthenticated connection was not closed by AuthSessionTimeout")
        }
    }
    finally {
        $idleClient.Dispose()
    }

    if ($process.HasExited) {
        $failures.Add("realmd exited during hostile-input smoke tests")
    }

    if ($failures.Count -gt 0) {
        throw "Realmd auth-stream smoke failures:`n - $($failures -join "`n - ")"
    }

    Write-Host "Realmd auth-stream smoke passed: prompt reject, fragmented challenge, live debug log, idle timeout, process survival."
}
finally {
    if ($null -ne $process -and -not $process.HasExited) {
        $tracked = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        if ($null -ne $tracked -and $tracked.Id -eq $process.Id) {
            Stop-Process -Id $process.Id
            $process.WaitForExit(5000) | Out-Null
        }
    }

    $resolvedTestRoot = [System.IO.Path]::GetFullPath($testRoot)
    if ($resolvedTestRoot.StartsWith($tempRoot, [System.StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolvedTestRoot).StartsWith("realmd-auth-stream-smoke-")) {
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
