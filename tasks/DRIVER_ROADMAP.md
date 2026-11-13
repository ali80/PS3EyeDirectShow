# PS3 Eye maintenance roadmap

Objective: a small, maintainable Windows camera integration with safe capture,
usable image controls, and documented application compatibility.

1. **Capture and installer foundation (initial branch).** Reject malformed formats
   and short samples; verify both architectures; keep installation limited to
   DirectShow filters; add CI and a scoped review. Finish with a reviewable PR,
   without replacing the working installed filter.
2. **Backend stability and dependency update.** Separate cancel signaling from
   transfer-callback cleanup; wake blocked frame readers on stop/error; make
   ownership and multiple-camera behavior explicit; update libusb. Acceptance:
   deterministic USB-error injection, no deadlocks under stop/unplug/reopen, and
   sustained 30/60 fps hardware capture without invalid writes or leaked handles.
3. **Image controls and measurement.** Expose exposure/gain and white-balance
   modes plus supported contrast/brightness/sharpness controls. Map standard
   DirectShow units correctly, validate ranges and define safe defaults.
   Acceptance: controls work in OBS, survive restart as intended, and measured
   image/latency comparisons justify defaults. Preserve native sensor limits.
4. **Media Foundation compatibility.** Build a proper source for Windows Camera
   and modern clients, with a tested Windows 11 registration/lifecycle strategy.
   Acceptance: Windows Camera can enumerate and capture; repeat open/close,
   format negotiation, unplug and privacy behavior are verified. Keep microphone
   capture on the separate USB audio path.
5. **Release gates.** Complete supported-Windows/app testing, clean-machine MSI
   install/uninstall/rollback and legacy migration testing, signing and dependency
   provenance. Publish a release only after those gates pass.

The first branch does not claim to complete steps 2–5. Hardware tests should not
save images/audio unless explicitly needed for an authorized measurement.
