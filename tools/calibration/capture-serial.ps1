param(
    [Parameter(Mandatory=$true)][string]$Port,
    [int]$Seconds = 90,
    [Parameter(Mandatory=$true)][string]$OutFile
)
$sp = New-Object System.IO.Ports.SerialPort $Port, 115200
$sp.ReadTimeout = 200
# TinyUSB CDC only transmits once the host asserts DTR.
$sp.DtrEnable = $true
$sp.RtsEnable = $true
$sp.Open()
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$buf = New-Object System.Text.StringBuilder
try {
    while ($sw.Elapsed.TotalSeconds -lt $Seconds) {
        try {
            $chunk = $sp.ReadExisting()
            if ($chunk) {
                # Stamp each flush so phases of the motion sequence can be
                # separated during analysis.
                [void]$buf.Append("`n#t=" + [math]::Round($sw.Elapsed.TotalSeconds, 2) + "`n")
                [void]$buf.Append($chunk)
            }
        } catch [TimeoutException] {}
        Start-Sleep -Milliseconds 50
    }
} finally {
    $sp.Close()
}
[System.IO.File]::WriteAllText($OutFile, $buf.ToString())
Write-Output ("captured " + $buf.Length + " chars over " + [math]::Round($sw.Elapsed.TotalSeconds) + "s")
