# PS3EyeDirectShow

Windows DirectShow source filters for the Sony PS3 Eye, in 32-bit and 64-bit applications.
This fork starts from [jkevin/PS3EyeDirectShow](https://github.com/jkevin/PS3EyeDirectShow)
and its Linux-derived [PS3EYEDriver](https://github.com/jkevin/PS3EYEDriver) backend.
Fork changes began on 2026-11-11. The original license and notices are retained.

The first changes harden media negotiation, sample writes, COM property handling,
and installer scope. They do **not** add resolution beyond the sensor's native
640x480, Windows Camera compatibility, or new image controls.

## Application compatibility and audio

| Use | Support |
| --- | --- |
| OBS and other DirectShow applications | Video through an existing WinUSB camera binding. |
| Video calls | Select PS3 Eye Universal when supported, or route OBS to OBS Virtual Camera. App support varies. |
| Windows Camera / Media Foundation applications | No Media Foundation source is implemented. DirectShow registration alone is insufficient. |
| Microphone | The USB audio interface uses the Windows USB Audio driver separately. |
| OBS Virtual Camera audio | Video only. Select the USB microphone separately in the calling app. |

Avoid opening the physical PS3 Eye simultaneously in multiple applications. OBS
Virtual Camera can serve a calling app while OBS owns the physical camera. Remote
Desktop audio redirection can hide the physical microphone from a session; device
detection alone does not confirm usable audio capture there.

For viewing/recording, start with 640x480 at 30 fps and good lighting. Higher frame
rates shorten available exposure time. Manual exposure/gain and white-balance
controls are planned; the current filter enables automatic gain and white balance.

## Build and verify

Requirements: Windows, PowerShell 7, Visual Studio 2022 C++ build tools, and a
Windows 10/11 SDK. Build commands do not register a filter or install software.

```powershell
git submodule update --init --recursive
pwsh -File scripts/build-and-test.ps1 -Platform x64
pwsh -File scripts/build-and-test.ps1 -Platform Win32
```

Each command builds the production DLL and runs 148 capture-contract checks using
the production pin and real DirectShow allocators. These tests need no webcam or
administrator access and save no images/audio. The tests fail against the original
upstream pin on truncated VIDEOINFOHEADER acceptance.

The build consistently uses the dynamic C runtime and enables stack cookies,
Control Flow Guard, DEP and ASLR. Pinned dependencies still need further review
and updating; see [the review](docs/REVIEW.md).

## Filter-only installer preview

After both architectures are built, use WiX 3.14.1 binaries:

```powershell
pwsh -File scripts/build-installer.ps1 -WixBinPath C:\path\to\wix314
```

This builds and validates `artifacts/installer/PS3EyeFilters-preview.msi`. It also
inspects the compiled package to require exactly the two filter DLLs and reject
USB-driver/certificate custom actions. It does not run the installer.

The preview **requires an existing compatible WinUSB binding**. It does not
provision USB drivers or add anything to Windows certificate stores. The original
legacy full installer did both; it is not the package built here. An existing
CL-Eye WinUSB binding is compatible, and its 32-bit filter is preserved.

This preview uses a separate upgrade family and blocks coexistence with the legacy
PS3 Eye Universal Driver MSI. Migration must first establish which package owns
the USB binding: uninstalling a legacy full package can remove that binding.
Clean-machine install, upgrade/rollback and signing are still release gates.
No signed production release of this fork has been published.

## Portable multi-camera capture helper

`scripts/build-and-test.ps1` also builds
`artifacts/<platform>/capture/ps3eye-capture.exe` and tests its limited-range YUV420
conversion. The helper enables the sibling Go recorder to select distinct PS3 Eyes
without registering a replacement DirectShow filter. It does not make multiple
PS3 Eyes appear as separate DirectShow webcams to other applications.

`--list` prints JSON USB port IDs without streaming. `--device <id>` captures only
that port at 640×480, 15 fps and writes binary YUV420 to stdout. It never falls back
to the first available camera. The owning recorder reads the stream, sends a byte
on stdin to stop cleanly, and terminates a stalled helper after a bounded timeout.
Do not run raw capture in an interactive terminal. It saves no files and plays no
audio. USB port identity depends on keeping cameras in the same physical ports.
The existing WinUSB driver binding is required on each camera.

This uses the existing native dependencies and retains their review limitations;
the helper is not a new kernel driver or a security certification.

## Further work

See [the scoped security/stability review](docs/REVIEW.md) and
[the implementation roadmap](tasks/DRIVER_ROADMAP.md). The major remaining items
are USB-disconnection recovery, dependency updates, image controls, and a Media
Foundation source for Windows Camera. Passing the current tests is not a security
certification or a substitute for those checks.
