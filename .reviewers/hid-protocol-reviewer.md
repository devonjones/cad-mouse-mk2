# hid-protocol-reviewer
**Checks:** TODO
**Spawn when:** TODO
**Contents:** What to check: Descriptor/struct consistency; Logical range discipline; TinyUSB API contracts; Pure-HID invariant; Report semantics · Do NOT flag

Review USB HID emulation correctness. This device impersonates a **3Dconnexion SpaceMouse Compact** (VID 0x256F, PID 0xC635); the driver (3DxWare) binds to it expecting genuine-hardware behavior.

**What to check:**

1. **Descriptor/struct consistency:** every HID report descriptor entry (report ID, REPORT_SIZE × REPORT_COUNT) must exactly match the packed struct sent for that ID. A size mismatch silently corrupts axis data on the host.
   - Report 1: translation, 3 × int16. Report 2: rotation, 3 × int16. Report 3: buttons, 32 bits total (2 used + padding).
   - Structs must stay `__attribute__((packed))`.

2. **Logical range discipline:** descriptor declares ±350; all values sent must be clamped to that range. Flag any path where an unclamped value can reach `sendReport`.

3. **TinyUSB API contracts:**
   - `sendReport` while `!usbHid_.ready()` fails — flag unchecked send paths
   - The IN endpoint carries ONE report per poll interval; multiple `sendReport` calls in one loop iteration fail after the first (this is why round-robin dispatch exists — flag changes that break it)
   - `TinyUSBDevice.task()` must keep being called from `loop()`

4. **Pure-HID invariant:** release builds must not add interfaces (CDC, vendor pages) or alter the descriptor shape — 3DxWare compatibility depends on matching genuine hardware. Debug-only additions must be fenced behind `ENABLE_DEBUG_SERIAL`.

5. **Report semantics:** deflection must stream (keepalive while non-zero) and end with a zero report on release; buttons only on change.

**Do NOT flag:**
- The VID/PID impersonation itself (that's the point of the project)
- Descriptor deviations from HID spec pedantry that match what real 3Dconnexion hardware ships
