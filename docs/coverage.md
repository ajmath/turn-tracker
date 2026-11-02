# Scenario traceability and implementation status

Automated tests are executable examples, not exhaustive verification. The same
core runs in firmware and simulation. Nothing in this table is physical evidence.

| Supported behavior | Automated evidence | Remaining validation |
| --- | --- | --- |
| 2–8 players, nonconsecutive MACs, discovery without authority | `Table::scenario`, each count 2–8, seeded faults | Real channel compatibility and RF range |
| Boot together / staggered boot | scenario initial startup; `abandoned` staggered node | USB/battery cold boot |
| First enrollment, coordinator explicitly selected, minimum/count/duplicate/capacity | scenario, `edgeScenarios`, nine-node capacity case | Recognizable rejection and capacity feedback |
| Expansion/reduction, arbitrary assigned-puck reuse, coordinator changes | scenario reduction, edges replacement and expansion | Three-puck physical reenrollment |
| Mistake correction by finalize and repeat | repeated enrollment in scenario/edges | User comprehension |
| Pending inactivity, restart, missing candidate recovery | scenario pending restart; `abandoned` 35-second inactivity and replacement | Actual NVS retention and slow reminder |
| Overlapping candidates withdraw and retry | `conflictScenario` simultaneous gestures, then successful setup | Partitioned overlap combinations not exhaustive |
| Debounce, held boot, release, threshold, clock wrap | `tests/core.cpp`, wrap table scenario | Exact every-ms boundary/mode-transition matrix remains incomplete; real bounce |
| Claim-to-enrollment during animation | edges drives real hold + immediate two taps | Physical timing/visibility |
| Claims preserve order, simultaneous claims | scenario/edges, `conflictScenario` | Actual callback arbitration latency |
| Enrollment/claim schedules, duplicate/delayed packets | all seeded runs, completion before subsequent inputs | LED-by-LED tolerance assertions and every late-schedule interruption case remain incomplete |
| Delayed distinct passes, retransmission, loss/duplication | scenario captures and replays old participant requests after claim; seeded links | Burst tapping on hardware; additional input cadence sweep |
| Final decision/adoption failure and retry | edges storage failure + partition + candidate reboot | Actual NVS flash failure behavior |
| Power cut before/after durable writes | 20 clean-network cases: candidate boundaries 1–6, each participant 1–2, old/new durable result | Exhaustive cross-product with every message boundary remains open |
| Stuck finalization replaced by explicit new selection | edges replacement flow after durable recovery | Replacement while multiple differently delayed old commits remain in flight needs broader exploration |
| Participant/coordinator/all restart | scenario, plus randomized actions | Physical power interruption |
| Old coordinator stays powered behind partition, heals, retires, reboots | edges | RF asymmetric partitions and longer absence |
| Stale/conflicting discovery is a hint only | injected Presence in `conflictScenario`; no adoption | Broader ambiguous-authority topology exploration |
| Member loss, asymmetric loss, healing, claim recovery | scenario and `conflictScenario` | Lease measured on real boards |
| Malformed/version/sender/length, storage corruption | focused core and injected unsupported version | Broader parser mutation/fuzz corpus |
| Queue pressure / bounded resources | 100-frame flood, 16-frame per-device service cap, 512-frame network cap | FreeRTOS queue overflow and peer-cache eviction on hardware |
| Randomized table actions | 30 seeded actions, taps/claims/reboots/partitions, convergence asserted after each | Optional longer seed sweep; arbitrary schedules not exhausted |
| Maintenance / diagnostic snapshots | firmware compiles, shared transport-independent formatter | Serial interactive bring-up |

## Replays and trace format

```sh
./scripts/test 42
build/table 42 clean
build/table 1
build/table 7
build/table 999
build/table 4294967295
./scripts/stress 100
```

Every failure reports the seed and scenario/boundary. Repeating the same seed and
mode replays the whole bounded suite, including its deterministic subcases. On
failure inspect `build/failure.trace`; successful three-puck main scenario writes
`build/example.trace`. `rx` lines show directed delivery, message kind, membership,
and revision; `persist` lines show durable transitions; tap/hold/restart lines show
physical actions; `table` lines show local LEDs, numeric roles/status, and queue
size every 500 ms. `*` means illuminated and `.` dark. The final flash and chase
can be read against those timestamps. A short checked-in example is in
`docs/example-table.trace`. Fault loss is 1/8 per transmission, occasional duplicate
at +250 ms, variable 10–200 ms delay, independent offsets/drift, deterministic seed.
A clean finalization boundary sweep models old-or-new durable write outcomes.

## Qualification limits / next work

This is an initial working prototype, not a claim that every requested fault
combination has been proven. Highest-priority gaps are the complete gesture
boundary matrix, packet-boundary/persistence cross-products, fully partitioned
competing final decisions, and stronger animation timing assertions. Existing
implementation pauses on missing participation, requires explicit reservation for
adoption, and never chooses authority from discovery votes. Expand model-based
exploration before declaring the full protocol qualified.

The NVS adapter must be checked by physical power cuts. Real RF, callback timing,
antenna placement, electrical behavior, LED current/brightness, charging, and
runtime remain completely unverified. Follow `hardware.md` in order.

## Interactive UI

`./scripts/ui` exposes the same native table through a local browser UI.
`./scripts/test-ui` drives the line-based native bridge and checks complete
gesture setup, pass, partition/healing, power-cycle recovery, storage faults,
asymmetric links, validation, and an exact replay against a fresh process.
Browser smoke testing covers enrollment, selection, finalization, and passing.
UI sampling is not an additional physical-timing validation.
