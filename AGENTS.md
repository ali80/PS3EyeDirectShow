# PS3 Eye driver work

- This checkout is the `ali80/PS3EyeDirectShow` fork. Keep changes here.
- Build and run regression checks before committing:
  `pwsh -File scripts/build-and-test.ps1 -Platform x64` and repeat with `Win32`.
- Keep generated output in ignored build directories or `artifacts/`.
- Do not install/register experimental binaries or change certificate stores as
  part of a build or test. Hardware tests must be explicit and must not save
  camera frames/audio by default.
- Keep dependency changes separate and preserve their license notices.
