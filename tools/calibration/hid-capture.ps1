param(
    [int]$Seconds = 10,
    [Parameter(Mandatory=$true)][string]$OutFile,
    [string[]]$Devices = @('VID_046D&PID_C628', 'VID_256F&PID_C6F0', 'VID_256F&PID_C635')
)
# Reads raw HID input reports from 3Dconnexion-style devices (shared read,
# alongside 3DxWare) and logs: t_ms device report_id v0 v1 v2 (int16 LE).
Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Collections.Generic;
using Microsoft.Win32.SafeHandles;
public static class HidCap {
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
  static extern SafeFileHandle CreateFile(string n, uint a, uint s, IntPtr sa, uint c, uint f, IntPtr t);
  [DllImport("hid.dll")] static extern bool HidD_GetPreparsedData(SafeFileHandle h, out IntPtr p);
  [DllImport("hid.dll")] static extern bool HidD_FreePreparsedData(IntPtr p);
  [DllImport("hid.dll")] static extern int HidP_GetCaps(IntPtr p, byte[] caps);
  static List<string> Lines = new List<string>();
  public static List<string> Errors = new List<string>();
  static object L = new object();
  public static string[] Snapshot() { lock (L) return Lines.ToArray(); }
  static System.Diagnostics.Stopwatch SW = System.Diagnostics.Stopwatch.StartNew();
  public static string Open(string path, string tag, int seconds) {
    var h = CreateFile(path, 0x80000000, 3, IntPtr.Zero, 3, 0, IntPtr.Zero);
    if (h.IsInvalid) return null;
    IntPtr pp;
    if (!HidD_GetPreparsedData(h, out pp)) return null;
    var caps = new byte[64];
    // HIDP_STATUS_SUCCESS
    int st = HidP_GetCaps(pp, caps); HidD_FreePreparsedData(pp);
    if (st != 0x00110000) return null;
    int len = BitConverter.ToUInt16(caps, 4);
    var fs = new FileStream(h, FileAccess.Read, len, false);
    var th = new Thread(() => {
      var buf = new byte[len];
      while (SW.Elapsed.TotalSeconds < seconds) {
        int n;
        try { n = fs.Read(buf, 0, len); }
        catch (Exception e) {
          lock (L) Errors.Add(tag + ": read failed at " + SW.ElapsedMilliseconds + " ms: " + e.Message);
          break;
        }
        if (n < 7) continue;
        string line = string.Format("{0} {1} {2} {3} {4} {5}", SW.ElapsedMilliseconds, tag, buf[0],
          BitConverter.ToInt16(buf, 1), BitConverter.ToInt16(buf, 3), BitConverter.ToInt16(buf, 5));
        lock (L) Lines.Add(line);
      }
    });
    th.IsBackground = true; th.Start();
    return tag + ": " + path + " reportlen=" + len;
  }
}
"@
$guid = '{4d1e55b2-f16f-11cf-88cb-001111000030}'
$opened = 0
foreach ($d in $Devices) {
    $dev = Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -like "HID\$d*" } | Select-Object -First 1
    if (-not $dev) { Write-Output "$d not present"; continue }
    $path = '\\?\' + ($dev.InstanceId -replace '\\', '#') + '#' + $guid
    $tag = if ($d -match '046D') { 'real' } else { 'diy' }
    $r = [HidCap]::Open($path.ToLower(), $tag, $Seconds)
    if (-not $r) { Write-Error "$d ($tag): open failed"; continue }
    Write-Output $r
    $opened++
}
if ($opened -eq 0) { Write-Error "no device opened; nothing captured"; exit 1 }
Start-Sleep -Seconds ($Seconds + 1)
$lines = [HidCap]::Snapshot()
[System.IO.File]::WriteAllLines($OutFile, $lines)
Write-Output ("captured " + $lines.Count + " reports")
# A device at rest sends nothing, so an empty capture is a warning, not a failure.
if ($lines.Count -eq 0) { Write-Warning "no reports received; was the puck moved?" }
foreach ($e in [HidCap]::Errors) { Write-Error $e }
if ([HidCap]::Errors.Count -gt 0) { exit 1 }
