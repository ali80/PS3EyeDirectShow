# PS3 Eye source review — 2026-11-11

Baseline: `jkevin/PS3EyeDirectShow` commit
`e778be1c94308e333907a83711564dbabb6bdd7f` (2019-02-22), PS3EYEDriver
`63ae5366f19a42ab7cf4d9bb6073378727d28756`, and libusb
`2a7372db54094a406a755f0b8548b614ba8c78ec` (1.0.22).

This is a focused source/build review of the DirectShow wrapper, installer,
camera packet/queue lifecycle, and dependency/build configuration. It is not a
complete audit of Windows, libusb, Microsoft base classes, or every downstream
application. The unsigned historical MSI has not been proven reproducible from
source. No remote-code-execution claim is made.

## Findings addressed in the first change

| Finding | Consequence | Change |
| --- | --- | --- |
| `CheckMediaType` accepted any positive format length before reading a full VIDEOINFOHEADER. | Short buffers can cause out-of-bounds reads; malformed metadata can be accepted. | Validate GUIDs, pointers, complete header size, bitmap fields, dimensions and frame interval before using them. |
| Allocation trusted `biSizeImage`; `FillBuffer` did not verify capacity before `getFrame` wrote a geometry-sized BGRA frame. | A small declared size or short sample can corrupt the hosting process's memory. | Compute frame bytes from accepted dimensions, canonicalize BI_RGB's optional zero size, check sample capacity and device frame size, and set actual data length. |
| Property methods dereferenced unchecked output pointers and omitted the returned byte count. | A malformed COM request could crash a client or break enumeration. | Handle size-only queries, null pointers and short property buffers explicitly. |
| Negotiation treated `QueryAccept` returning S_FALSE as acceptance; some capability fields were wrong or overflowed. | Inconsistent format negotiation and invalid capabilities. | Require S_OK, reject live format changes, check allocation errors, initialize capability output and use bounded bitrate values. |
| No physical camera was treated as successful startup. | A missing camera could produce misleading blank output in a busy loop. | Fail stream startup with a device-not-connected error. |
| The full installer added a bundled self-signed certificate to LocalMachine Root and TrustedPublisher stores and provisioned WinUSB. | An application filter installation expanded machine-wide code-signing trust and could affect USB-driver ownership. | Build a filter-only MSI; remove certificate/driver custom actions and unrelated DRM_RNG registry writes. Detect and block automatic legacy-package migration. |
| libusb used the static C runtime while other components used the dynamic runtime. | Conflicting runtime libraries and inconsistent memory/runtime behavior. | Use a common runtime through an imported build property file; enable CFG, stack cookies, DEP and ASLR. |

The format/COM issues live inside the caller's process. Ordinary local clients
already execute in that process; these findings primarily establish memory safety
and reliability defects, rather than a demonstrated cross-process privilege
boundary. Whether remotely negotiated parameters can reach them depends on the
hosting application and was not established in this review.

The historical certificate is a code-signing certificate. The review did not find
its private key in the repository and does not claim it permits arbitrary TLS
interception. Its trust-store installation is nevertheless outside the scope of a
DirectShow filter and is removed from the new package. A filter-only installation
of the historical MSI skips that feature.

## Significant remaining work

1. **USB disconnect/error recovery.** In `PS3EYEDriver/src/ps3eye.cpp`, the libusb
   callback calls `close_transfers()`, which waits for cancellation callbacks on
   the same event-processing thread. `FrameQueue::Dequeue()` also waits without
   a shutdown predicate. Error handling can deadlock, and a blocked frame read can
   prevent clean stop/unplug. This is confirmed by control-flow inspection, not
   a destructive physical-unplug test. Fix in a separately reviewed backend
   change with fault injection and wake/cancel tests.
2. **Dependency maintenance.** libusb 1.0.22 and the old DirectShow base classes
   remain pinned. Review applicable advisories, migrate to a supported libusb
   release, and test WinUSB compatibility. Age alone is not proof of a specific
   exploitable CVE on this Windows path. Legacy compiler warnings remain.
3. **Concurrency and renegotiation.** Device enumeration/state is shared; multiple
   filters, reconnect failures and concurrent property changes need dedicated
   tests. The first change does not redesign that lifecycle or guarantee
   multi-client physical-camera access.
4. **Image quality controls.** The backend supports gain, exposure, sharpness and
   color controls but the DirectShow wrapper forces automatic gain/white balance
   at startup and exposes no standard image-control interfaces. Add validated
   standard controls with documented ranges and per-camera settings. Measure
   low-light noise, motion blur, color fidelity, latency and dropped frames.
5. **Windows Camera support.** DirectShow and Media Foundation are distinct APIs.
   Implement a Media Foundation source/Windows virtual-camera integration with
   correct lifecycle, timestamps, format negotiation and registration. Merely
   adding a registry entry or routing through a DirectShow virtual camera is not
   an implementation of that support.
6. **Release engineering.** Signing, full dependency/license inventory, repeatable
   release artifacts, clean-machine install/uninstall/rollback, existing CL-Eye
   coexistence and legacy migration remain gates. The preview MSI is compiled and
   ICE-validated, but has not been installed as a production replacement.

No networking/telemetry was found in the reviewed wrapper and camera-source
paths. That statement does not cover host applications or certify all dependency
code. WinUSB access is not a standard Windows camera capture path; do not assume
all Windows camera privacy controls or indicators enforce the same behavior.

## Verification

- Production DLLs built with Visual Studio 2022/v143 and Windows SDK 10.0 for x64
  and Win32; 148 capture-contract checks passed on each architecture.
- The same suite built against the unmodified upstream pin failed at check 7 on
  acceptance of a truncated VIDEOINFOHEADER, establishing a regression baseline.
- Real DirectShow allocators exercise undersized and oversized samples, canonical
  image sizes, output bounds, null pointers, properties and advertised formats.
- x64 PE load configuration confirms CFG, DEP, ASLR and high-entropy ASLR.
- The filter-only MSI compiled, passed ICE validation, and was decompiled to
  check its two-DLL payload and absence of certificate/USB-driver actions.
  WiX decompilation emitted stock UI-table warnings; payload validation passed.
- The currently installed historical filter delivered changing 640×480 frames
  and an OBS preview. This is **not** hardware validation of the new fork binary.
- No experimental filter was installed or registered by the build/tests.

Fuzzing, sanitizer runs, sustained hardware capture, microphone capture, unplug
fault injection and the broader compatibility matrix are not covered by these
results.
