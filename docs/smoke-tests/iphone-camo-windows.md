# iPhone camera through Camo on Windows

This smoke test uses an iPhone as a regular OBS video-capture source. It is
useful when a physical webcam or PTZ camera is not available.

## Install and connect

1. Install **Camo Studio for Windows** from the official Camo download page.
2. Install **Camo Camera** on the iPhone.
3. Connect the iPhone to Windows with USB first. Unlock the phone and accept
   **Trust This Computer** if iOS asks.
4. Open Camo Camera on the iPhone and Camo Studio on Windows. Select the iPhone
   and confirm that its live preview is visible.
5. Keep the phone stable in landscape orientation with the rear camera aimed at
   the service area. USB is preferred for a fixed church installation; Wi-Fi
   can be used after the USB path works.

## Add the source to OBS

1. In **Sources**, add **Video Capture Device**.
2. Select `Camo Camera` (the exact label can include `Camo`).
3. Use 1280x720 or 1920x1080 at 30 FPS, depending on the available USB and CPU
   headroom.
4. Add the `Church Director` filter to this Camo source. The filter and its
   `Toggle` hotkey must belong to the same parent source; a hotkey assigned to
   an unrelated `Image` source will not control the camera source.
5. Leave **Synthetic** unchecked for real iPhone video. Leave tracking disarmed
   until the operator is ready, then use the configured `Toggle` key to arm it.
6. Map the same keyboard key to the PTZ controller macro when the operator needs
   a physical arm/disarm button.

Church Director performs virtual pan/crop/zoom inside OBS. It does not physically
   move the iPhone or send VISCA/IP/serial commands to a PTZ head.

## Verification

- The OBS preview shows the live iPhone feed before the filter is armed.
- Pressing the configured toggle key once logs `Church Director: tracking ARMED`.
- Pressing it again logs `Church Director: tracking DISARMED` and returns the
  source to pass-through behavior.
- With a real person in view and tracking armed, the filter can change the
  virtual crop. A static image or a person inside the dead-zone may show no
  visible movement even though the hotkey is working.

## Troubleshooting

- If `Camo Camera` is missing, unlock the iPhone, keep Camo Camera open, retry
  the USB connection, and accept the trust prompt.
- Close other applications that may already be using the phone camera.
- Test USB before Wi-Fi. For Wi-Fi, keep the phone and PC on the same network.
- If the log shows `ARMED`/`DISARMED` but the image does not move, verify that
  the filter is on the live Camo source, **Synthetic** is off, and the person is
  outside the dead-zone. The detector is not a face recognizer.

Official references:

- [OBS smartphone camera guide](https://obsproject.com/kb/smartphone-camera-guide)
- [Camo Studio downloads](https://camo.com/downloads)
- [Camo Camera on the App Store](https://apps.apple.com/us/app/camo-camera/id1514199064)
