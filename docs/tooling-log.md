# Tooling needs and installation log

## Enclosure CAD exports (2026-11-02)

STL generation uses Manifold, trimesh, NumPy and Matplotlib, pinned separately in
`hardware/enclosure/requirements-cad.txt`. They were installed into an isolated
temporary environment at `/private/tmp/turn-tracker-cad-venv`; no machine-wide
software or firmware environment was changed. `generate.py` emits matching SCAD
and STL geometry, checks modeled collisions and mesh closure, and renders a
preview. OpenSCAD itself is not installed; its generated source has not been
executed in OpenSCAD. Tinkercad imports and physical component fits remain to be
checked by the user.

Keep this file current as implementation discovers requirements. Do not install new
machine-wide software automatically. Review and batch installations with the user,
using each toolchain's appropriate manager. There is intentionally no Brewfile.

## Requirements

| Tool | Purpose | Preferred management | Current status (2026-11-02) |
| --- | --- | --- | --- |
| Xcode Command Line Tools / Apple Clang | Native C++ tests and simulator; macOS SDK | Apple's installer / Software Update | Ready: Apple Clang 21.0.0; native checks pass |
| uv | Python/runtime and isolated dependency management | Existing uv installation; upstream installer if needed | Already present, version 0.9.26; no installation needed |
| Python 3.12.12 | PlatformIO runtime | uv-managed Python, isolated `.venv` | uv-managed Python 3.12.12 provisioned; Homebrew Python removed |
| PlatformIO Core 6.1.18 | Firmware build/upload/serial tools | `uv pip sync` in repository `.venv`; exact dependencies in `requirements.txt` | Already provisioned by earlier bootstrap |
| Espressif32 6.10.0 + Arduino framework 3.20017.0 | ESP32-C3 framework and cross-compiler | PlatformIO package manager; `platformio.ini` pins | Downloaded; firmware compilation passed |
| VS Code + PlatformIO/C++ extensions | Editor, shared tasks | Official VS Code distribution and extension marketplace | VS Code app is installed; extension installation not inspected |

No additional installation is currently requested. Firmware builds use PlatformIO's
cross-compiler. Host tests use `c++` (Apple Clang on macOS), with an optional `CXX`
override. Standalone Homebrew LLVM is not required.

## Changes already made before the installation-policy change

Earlier authorized Homebrew installs completed in the background:

- Python 3.12.16 installed, along with Homebrew dependency changes.
- LLVM 23.1.4 installed, with z3; Homebrew also installed Git 2.56.0_2 and dependencies.
- Cleanup completed at the user’s request: LLVM, z3, Homebrew Python 3.12,
  and its unused mpdecimal dependency were uninstalled. Homebrew Git is retained
  for repository work. Existing shared Homebrew dependencies were not mass-removed.
  This repository no longer relies on Homebrew paths.
- A project-local `.venv`, `.platformio`, and `.pio` were created; all are ignored.

Homebrew LLVM still requires a macOS SDK; installing it did not replace the need
for Apple's Command Line Tools.

## Commands after prerequisites are ready

```sh
./scripts/bootstrap          # explicit: uv creates .venv and syncs pinned packages
./scripts/pio run            # builds; PlatformIO downloads missing target packages
./scripts/pio run -t upload  # auto-detects board; optional --upload-port supplied locally
./scripts/pio device monitor
./scripts/test              # native tests + seeded discovery simulation
```

Bootstrap is an explicit provisioning step. It does not install uv, VS Code,
Apple tools, or extensions. Normal host tests never install software. CI provisions
uv 0.9.26 and invokes the same bootstrap/build/test scripts.

Reference: [uv environment management](https://docs.astral.sh/uv/pip/environments/).

## Validation log

- Firmware build passed before this policy update: 37,604 bytes RAM,
  703,626 bytes flash. This validates compilation of the discovery foundation only.
- Apple tools are now ready. Focused host checks and seeded full-table scenarios
  pass; see `coverage.md` for qualification gaps.
- Project `.venv` was recreated with uv-managed Python 3.12.12 and the exact
  requirements synchronized successfully. Shell scripts pass syntax checks.

## Current implementation checks

- Latest firmware compile passed: 38,836 bytes RAM; 717,318 bytes flash.
- Apple Clang focused tests and full-table suites passed for seeds 42, 1, 7,
  999, and 4294967295, including 2–8 players and 20 persistence-cut subcases
  per seed. Clean-link mode also passed.
- AddressSanitizer/UndefinedBehaviorSanitizer checks passed on the focused tests
  and the seed-42 table suite.
- VS Code JSON and shell syntax validated. CI is configured but has not been
  executed remotely by this task. No boards have been flashed or tested.

## Interactive harness UI

No new tooling or packages required. `scripts/ui` uses existing Apple Clang to
compile `sim/ui.cpp`, then existing uv-managed Python's standard-library HTTP
server to serve `sim/ui/` on loopback. Browser UI is buildless HTML/CSS/JavaScript.
The native bridge and CLI suite share `sim/table.hpp` and the unchanged firmware
core. `scripts/test-ui` checks real gesture setup, passing, recovery, storage faults,
directed links, input validation, and deterministic command replay.

UI validation passed: native bridge/replay tests, existing core/table regression
suite (seeds 42, 1, 7, 999, 4294967295), JavaScript syntax, Python compilation,
and shell syntax. Browser smoke test completed real enrollment, ordered selection,
finalization, and passing; the local UI was left open on a paused three-puck game.

## Enclosure STEP reference update (2026-11-03)

Added cadquery-ocp 8.0.1.1.0 (and its VTK dependency) only to the temporary CAD
environment to read the user-supplied STEP. Extracted PCB/socket bounds and
normalized reference STLs. Revised exports pass board/base insertion sampling
(97 positions per option), collision checks, and printable STL reload checks.
The reference model and measured bounds are saved under hardware/enclosure/reference.
No machine-wide install or firmware environment changes.

## Enclosure reinforcement after user test print

User reported that the committed retaining bar printed and worked; component fit
is still unconfirmed while awaiting parts. Switch walls are tripled outward while
preserving interior faces. Socket interior geometry and the bar are restored
exactly from the committed design. Only socket roofs are removed and rear walls
are increased outward from 0.6 to 1.8 mm. Bar STLs and shells are byte-identical
to their committed files. Option 1 shelf underside is filled solid across its
full width. Outside diameters remain 90 / 70 mm. Option 2 battery placement is
explicitly deferred by the user and excluded from fit qualification; any known
reference-envelope intersections are recorded. All other collision/insertion
checks remain enforced. Revised component fit remains unconfirmed.

## First component fit adjustments

Components have arrived. Added 0.2 mm per PCB-contact face in base channels and
bar notch, plus 0.2 mm front-stop clearance. Restored the sloped retaining roof
while keeping the flat socket roof open. Widened the switch pocket 0.4 mm, relieved
its rear face 0.2 mm, shortened long bracket arms 1 mm and moved brackets 1 mm
outward. Nominal diameters and shells remain unchanged. These new clearances
need another physical fit check. Geometry checks verify sloped-roof presence,
flat-roof removal, board/bar insertion and export integrity.
