# Calibration tooling

Per-unit motion calibration workflow for the SpaceMouse-emulation firmware.
The values in `firmware/include/Config.h` (DECOUPLE, TRIM_POS/NEG, DEAD_AXIS)
were fitted for one specific unit — a different build should re-run this.

## Workflow

1. Build and flash a debug firmware (adds a USB CDC serial port with
   telemetry):
   `PLATFORMIO_BUILD_FLAGS="-DENABLE_DEBUG_SERIAL" pio run`
2. Capture telemetry while performing the guided motion sequence
   (rest 5s, then each axis to full deflection in both directions,
   ~2s holds: slide right/left, slide away/toward, pull up/press down,
   tilt back/forward, rock right/left, twist cw/ccw):
   `powershell -File capture-serial.ps1 -Port COM4 -Seconds 90 -OutFile capture.txt`
   (DTR must be asserted or TinyUSB CDC sends nothing — the script does this.)
3. Inspect the capture: `python3 analyze.py capture.txt`
   (per-axis extremes, 1s-bin timeline, rest-phase noise floors).
4. Fit the decoupling matrix + trims: edit the phase time windows in
   `fit_matrix.py` to match your capture's timeline, then
   `python3 fit_matrix.py capture.txt` and paste the emitted arrays into
   `Config.h`.
5. After flashing the fitted firmware, capture the same sequence again and
   use `fit_crossfix.py` (adjust its trim constants and phase windows first)
   to fit the residual direction-dependent CROSS_POS/CROSS_NEG corrections.
6. Rebuild without the debug flag for the final pure-HID firmware.

Flashing without opening the case: hold both buttons 10s to reboot into the
UF2 bootloader. Hold both 3s to recalibrate the zero pose. Hold only the
right button 3s (while idle) to cycle the LED ring color.
