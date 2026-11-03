# Wireless turn tracker

Identical Arduino/ESP-NOW firmware for Seeed Studio XIAO ESP32-C3 pucks, with a
shared deterministic C++ core and simulated table. No router, phone, cloud, BLE,
or automatic coordinator election is involved. Supports orders of 2–8 players.

**Status:** initial implementation, under simulation validation. Do not treat this
as hardware-qualified firmware. See [coverage](docs/coverage.md) for implemented
flows and limitations, and [tooling log](docs/tooling-log.md) for prerequisites.

## Development on macOS

Use Apple's Command Line Tools for host C++. Use uv for Python and the local
PlatformIO environment. There is no Brewfile. New machine-wide tooling needs are
recorded before installation. After reviewing prerequisites:

```sh
./scripts/bootstrap
./scripts/pio run
./scripts/test
build/table 7                  # deterministic loss/duplication/delay replay
build/table 7 clean            # no network faults
./scripts/stress 100           # optional longer seeded sweep
```

Bootstrap explicitly provisions uv-managed Python 3.12.12 and exact packages in
`requirements.txt`. PlatformIO downloads its own ESP32 cross-compiler into ignored
`.platformio/`. VS Code tasks invoke these scripts; install recommended extensions
from `.vscode/extensions.json`. The VS Code extension's own build buttons are not
the canonical pinned commands; use the shared tasks. Built-in Core/Python installs
are disabled. For optional PlatformIO extension panels, quit VS Code and launch it
from a shell with the local tooling on PATH:

```sh
PATH="$PWD/.venv/bin:$PATH" PLATFORMIO_CORE_DIR="$PWD/.platformio" code .
```

The `code` shell launcher is optional and can be enabled through VS Code's command
palette. Shared tasks work without it. [PlatformIO's IDE configuration](https://docs.platformio.org/en/latest/integration/ide/vscode.html)
documents its Core/Python settings.

PlatformIO Core 6.1.18, Espressif32 6.10.0, Arduino 2.0.17, ESP-IDF 4.4.7.
See [version evidence](docs/versions.md). Host tests require C++17; optionally set
`CXX` to a different compiler. Builds and CI use the same scripts.

## Interactive virtual table

```sh
./scripts/ui                 # compile the native bridge and serve on 127.0.0.1:8765
# Optional: ./scripts/ui --port 8766
```

Open [the local virtual table](http://127.0.0.1:8765). No additional packages or
software installs are needed. This is a local developer tool; it is not hosted.

- Click/hold the large puck buttons, or use Tap, Hold 3s, and Start enrollment.
  Shortcuts schedule real physical input through the shared gesture recognizer.
- Run/pause, change speed, or step 100 ms / 1 second. Paused inputs only execute
  as simulation time advances. Use 1× speed to observe the actual feedback cadence.
- Toggle power, restart, fail storage writes, isolate pucks, or block individual
  directed links. Radio faults enable the existing seeded loss/duplication/delay.
- Inspect live membership/order, revisions, storage writes, LED history, and events.
- Export/load a deterministic command replay, or save the recent text trace.
  Start a New table before recording; reopening an already-running session cannot
  reconstruct commands issued before the page loaded. Replay export caps at 10,000
  commands; trace keeps the most recent 5,000 lines and LED history 300 samples.

Each server has one shared in-memory table. Power cycles preserve simulated durable
storage; New table clears it. Stop the server with Ctrl-C. Keyboard Space/Enter on a
focused large puck button presses/releases it; losing focus releases manual input.
The browser displays the native C++ simulator's outputs, not a JavaScript copy of
its protocol. The Python standard-library HTTP adapter binds only to loopback.

Run bridge/replay checks with `./scripts/test-ui`. This requires bootstrap's Python
environment and Apple Clang, the same existing prerequisites as the other tools.

## Flash and inspect

Attach the supplied antenna before radio tests. Connect one board with a USB data
cable, then:

```sh
./scripts/pio device list
./scripts/pio run -t upload
./scripts/pio device monitor
```

Repeat for all boards with the same image. If autodetection is ambiguous, pass
`--upload-port /dev/cu.YOUR_DEVICE` locally; never commit a serial-port name.
Monitor defaults to 115200 baud; pass `--port` to select a specific device.
Close other serial monitors before uploading. If upload fails, use the board's
BOOT/reset sequence from Seeed's guide, check the cable and port, and retry.

Send newline-terminated USB commands:

- `?` or `snapshot`: JSON status, IDs, revision, active position, order, and counters.
- `log`: last 64 gesture/protocol/persistence/animation events.
- `number 3`: optional persisted human label, unrelated to leadership or order.
- `forget`: maintenance only; clears local saved state and permits new enrollment.

Battery is reported as `null`: the baseline has no battery sensing circuit.
No charging state or battery percentage is inferred.

## Table controls

Wait until enrollment feedback is visible on the devices before selecting them.
On any puck, hold for at least three seconds until threshold feedback, release,
then complete two short taps within 1.5 seconds. That puck is the candidate
coordinator. **This gesture does not select it.** Tap each puck in the desired
circular order, including the initiating puck at its desired position. Wait for
each selection acknowledgment before tapping the next puck; accepted request
arrival order determines the saved order under packet loss.

Finish by holding for three seconds and releasing **on that same initiating
puck**. At least two devices, including the initiator, must have been selected.
Invalid finalization preserves selections. Duplicate selections preserve order.
Selections are durable before acknowledgment. Inactivity retains progress.

Finishing setup retries automatically. When all selected devices have persisted
membership, a chase follows selection order at one-second intervals, all flash,
and the first player stays lit. The active player's short tap passes on release.
Inactive ordinary taps do nothing. A hold/release claims the turn without changing
saved order and runs a half-second chase starting at the claimant.

The two taps immediately following a hold remain available during claim animation
to start enrollment. Other ordinary animation taps are consumed. A lone tap in
the escape window is consumed. Long holds never also produce short taps.

Correct an order mistake by swapping physical pucks, or finish and enroll again.
There is no undo, separate pairing gesture, or nested enrollment restart control.

## Recovery and light patterns

One short pulse every two seconds: unassigned / discovery / waiting for claim.
One short pulse during enrollment: awaiting selection or saved selection reminder;
a selection acknowledgment lights briefly. Finishing setup blinks at 400 ms.
Paused shows two short pulses every two seconds. Conflict/storage error blinks
rapidly. A steady light after animation is the confirmed current turn. Threshold
feedback is steady while held. Context distinguishes reused single-color patterns.

A missed packet retries without extra taps. Sustained member loss pauses the
whole group; reconnecting normally restores a known turn automatically. After a
coordinator reboot the saved order remains, but a hold/release must claim a turn.
Participant reboot normally recovers the current turn without requiring a claim.
There is no automatic failover. With a permanently missing coordinator or member,
start fresh enrollment using the devices present. A missing candidate's selected
pucks retain suspension; after its invitation expires they can use the normal
hold/two-tap gesture or be explicitly selected by another candidate.

If overlapping enrollment causes rapid conflict feedback, release controls and
wait at least three seconds for contenders to withdraw, then retry from one puck.
Radio partitions can hide competing attempts; see protocol limitations. No timeout
rolls back a durable final decision. A fresh explicit enrollment supersedes stuck
finalization. USB commands are not required for normal table recovery.

## Wiring and first hardware tests

Read [hardware bring-up](docs/hardware.md) before connecting the battery or button
LED. Defaults: button D2/GPIO4 to GND with `INPUT_PULLUP`; LED driver D3/GPIO5,
active high. Configure `BUTTON_PIN`, `LED_PIN`, `LED_ACTIVE_HIGH` with build flags.
The illuminated button LED must use a verified current-limited driver, not a GPIO
connection. Keep the switch contacts separate from the LED terminals.

Start with USB serial, switch input, and a current-limited LED/driver bench test.
Then test two radios, three-puck enrollment/play/claims/animations, and power-loss
recovery. Only then connect verified battery wiring and measure charging/runtime.
The battery switch must be on to charge. USB may power the board with it off.

Simulation cannot establish electrical safety, switch behavior, RF range,
brightness, real timing, charging compatibility, or battery runtime.

## Printable enclosures

Two centered-button puck designs, printable STLs, editable CAD sources, component
references and Tinkercad import instructions are in [hardware/enclosure](hardware/enclosure/README.md).
The designs are geometry-checked prototypes; physical component fits remain unverified.
