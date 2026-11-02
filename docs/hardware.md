# Hardware bring-up and measurement record

All checkboxes are **outstanding**. Compilation and simulation are not hardware
evidence. Record exact part links, board revision, instruments, measured values,
firmware version, date, and observations when completing each step.

## Power and wiring

- [ ] Confirm exact protected single-cell LiPo documentation: nominal 3.7 V,
  full 4.2 V, stated 1100 mAh, allowed charge rate 1C = **1.1 A maximum**.
  These are supplied assumptions, not independently verified product ratings.
- [ ] Verify the actual battery/connector polarity with a meter before soldering.
  Neither red/black wire colors nor 1.25 mm connector shape establish polarity.
- [ ] Check exact SS12F15 switch and pigtail specifications against measured load;
  no current, voltage, or contact rating is assumed from the part name.
- [ ] Battery positive → slide switch common/selected throw → board BAT+;
  battery negative → BAT−/GND. Insulate unused switch terminal and solder joints.
  No software-off and no external charger are implemented.
- [ ] Verify actual board charger configuration. Seeed documents approximately
  380 mA fast / 40 mA trickle for this board; measure the purchased board and check
  the actual battery's allowable charging conditions. 1.1 A is the stated battery
  limit, **not** this board's charge current.
- [ ] Battery switch **on** for charging. With switch off USB can still power the
  board; the switch is not a complete system disconnect. Check voltage/backfeed
  behavior under USB-only, battery-only, and both supplies.

Primary references: [Seeed guide](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/)
and [board schematic](https://files.seeedstudio.com/wiki/XIAO_WiFi/Resources/Seeeduino-XIAO-ESP32C3-SCH.pdf).
Exact battery, switch, connector, and Gebildet button listings remain unverified.

## Button and LED driver

- [ ] Identify the normally-open switch contacts separately from LED terminals
  using continuity/diode tests with power removed. Confirm terminal polarity.
- [ ] Switch: D2/GPIO4 to GND, internal pull-up. LED control: D3/GPIO5.
  These match the actual pinned Arduino board variant. Avoid GPIO2, GPIO8, GPIO9
  strapping connections; test holding the button during boot. Configurable build
  flags are `BUTTON_PIN`, `LED_PIN`, `LED_ACTIVE_HIGH`.
- [ ] Do **not** power the illuminated button directly from GPIO. The listing's
  claimed 3–9 V range does not establish current or built-in resistance.
- [ ] Bench-test the LED with a current-limited supply. Establish polarity, forward
  voltage/current curves, and whether an internal resistor exists. Start at a low
  current limit and increase only within verified component limits.
- [ ] Driver option: low-side logic-level N-MOSFET rated for 3.3 V gate drive;
  source → GND, drain → LED−, LED+ → **switched battery positive through verified
  series current limiting**, gate → GPIO through a small gate resistor, gate to
  GND through a pull-down. Choose resistor, MOSFET, and thermal ratings from actual
  LED current/voltage measurements and component datasheets.
- [ ] For an LED needing external series resistance, calculate worst-case current
  at 4.2 V using minimum measured forward voltage: `R >= (4.2 - Vf_min) / I_target`;
  verify resistor dissipation and brightness at low battery voltage. If internal
  limiting exists, verify its current across the entire battery range. Do not
  omit current limiting based on the advertised voltage range.
- [ ] This switched-battery LED supply can be dark on USB with battery switched
  off. Use a separately verified current-limited bench supply for initial USB LED
  tests; never connect two power rails together to make it illuminate.
- [ ] Confirm active polarity, boot-off behavior, real contact bounce, debounce,
  threshold feedback, and brightness under table lighting.

## Staged functional validation

1. [ ] USB enumeration, upload identical image, serial snapshots and bounded log.
2. [ ] Button-only input, held-button boot, threshold/window timing and wrap-safe
   host tests; verify actual physical bounce with traces/instrumentation.
3. [ ] Current-limited LED driver bench test, then pattern recognition by users.
4. [ ] Two-board station MAC discovery, common channel, unencrypted ESP-NOW;
   measure callback/queue load, loss, RF range, and packet latency.
5. [ ] Three-board enrollment: initiator not implicitly selected, arbitrary order,
   duplicate taps, invalid finalization, pending inactivity and restart.
6. [ ] Passes, claims, saved order, claim-to-enrollment escape, both chase speeds,
   final flash, tap consumption, and human-visible timing tolerance.
7. [ ] Power removal at selection save, final decision, adoption, and animation;
   reboot individual/all boards. Verify no extra hold after a committed decision.
8. [ ] Shield/disconnect a participant, then coordinator; verify stale steady
   indications clear, table pauses, healing recovers or explicitly awaits a claim.
   Test old coordinator returning after replacement, including while powered.
9. [ ] Verify battery wiring and charger measurements before battery operation.
10. [ ] Log idle/active current and a measured full battery runtime at realistic
    brightness and RF conditions. Baseline radio is always listening. No runtime
    promise and no coordinated sleep are made.

## Optional ADC circuit (disabled)

No ADC circuit or battery telemetry is enabled in this image. Before implementing:
choose an ADC-capable non-strapping pin, divider ratio/resistor values, filtering,
calibration, and attenuation using the ESP32-C3 ADC specifications. Ensure the
worst-case full battery voltage, resistor tolerances, and transients stay inside
ADC limits (not merely 3.3 V). Connect to the switched side and inspect protection
and USB/backfeed paths so the divider cannot bypass the battery switch or power
an unpowered GPIO. Report measured voltage and coarse status only; report
unavailable until explicitly enabled. Do not infer percentage or charging state.

## Backlog

Hardware qualification above; optional ADC hardware/telemetry; protocol fault
coverage gaps in `coverage.md`; transport-independent diagnostic formatter;
read-only BLE GATT diagnostics and iPhone viewer later. Custom PCB work remains
out of this implementation's scope.
