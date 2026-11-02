# DIY wireless turn tracker: firmware, tooling, and simulated table

Implement the initial firmware and repository-driven developer tooling for a DIY wireless board-game turn tracker.

Inspect the repository and read applicable AGENTS.md instructions first. Implement the working foundation, run available checks, and document hardware validation that remains outstanding. Make reasonable implementation decisions without stopping for routine clarification. Treat the simulation harness as a first-class deliverable developed alongside the protocol.

## Product and scope

Each player has a battery-powered puck with one illuminated momentary button. The active player's light is on; tapping it passes the turn.

Initial hardware is three pucks. Support 2–8 players. Verify the actual bundled ESP-NOW transport limits and account for the broadcast peer entry. Use unencrypted ESP-NOW; encryption is not required. Do not silently reduce the supported player count without reporting the concrete limitation.

Operate without a phone, router, internet, or cloud service. Assume one intended game and one intended enrollment process within radio range. Do not implement simultaneous nearby-game support, automatic round detection, automatic starting-player rotation, or automatic coordinator election/failover.

Any collection of compatible pucks can form a new membership regardless of previous membership. All run the same firmware binary. No membership or coordinator identity is hard-coded.

Optimize for low friction at the table. Routine packet loss should recover automatically. Recovery must be part of the protocol design, not a collection of special-case patches. Physical controls should remain simple even when internal recovery is sophisticated.

## Prototype hardware

Per puck:

- Seeed Studio XIAO ESP32-C3.
- Protected single-cell LiPo, stated as 3.7 V nominal / 4.2 V full, 1,100 mAh, maximum charge rate 1C.
- XIAO built-in USB-C charger.
- Two-pin 1.25 mm battery connector pigtail soldered to battery pads.
- SS12F15 SPDT slide switch inline with battery positive.
- Gebildet 12 mm illuminated normally-open momentary pushbutton with separate switch and LED connections. Listing claims 3–9 V illumination; LED current and internal resistance remain unverified.

Treat exact battery, switch, connector, and button specifications as supplied assumptions until supported by exact product documentation or measurement. Do not invent part ratings. Verify connector polarity rather than inferring it from wire color or connector shape. Verify board charging current against the actual battery specification; 1C for 1,100 mAh means 1.1 A, not the board's charge current.

The battery switch must be on for charging. USB can still power the board with the battery switch off; this is not a complete system power disconnect. Do not implement software-off or add an external charger.

Do not assume the LED is safe for GPIO drive. Document measurement and a transistor/resistor driver option, including the supply and current-limiting arrangement. Keep pins and LED polarity configurable. Verify board pin mappings and avoid strapping-pin conflicts, including when a button is held during boot.

Optional battery sensing uses an explicitly enabled resistor-divider/ADC circuit. Review ADC voltage limits and possible paths around the battery switch. Report voltage and coarse battery status only when hardware is enabled; otherwise report unavailable. Do not fabricate percentage or charging state.

Use an always-listening radio baseline for v0. Measure battery runtime; do not promise a runtime or introduce coordinated deep sleep. Future custom PCB work is out of scope.

## Repository-driven developer tooling

Use VS Code, PlatformIO, Arduino framework, and C++.

- Pin a verified compatible PlatformIO platform release and PlatformIO Core in bootstrap and CI.
- Pin external dependencies to exact versions or commits.
- Document the resolved Arduino core and ESP-IDF versions and use their actual APIs and bundled configuration.
- Verify version choices using primary sources; record evidence rather than inventing pins.
- Provide repeatable native macOS setup, build, upload, monitor, and test commands.
- Commit platformio.ini, shared VS Code tasks, and extension recommendations. Ensure documented tasks use the intended pinned tooling.
- Avoid machine-specific paths and committed serial-port names.
- Provide CI firmware compilation and deterministic host tests, plus a bounded seeded simulation suite.
- Ignore generated builds, downloaded dependencies, and toolchains.
- Keep local and CI commands consistent. Arduino IDE is not required.

## Terminology and authority

- Node/device: physical puck, identified by its station-interface MAC address.
- ESP-NOW peer: transport entry, distinct from membership.
- Discovery: broadcasts and replies locating devices and current authority.
- Enrollment: deliberate selection of nodes and order.
- Candidate coordinator: device that initiated the current enrollment.
- Coordinator: that same device after membership finalization; owns authoritative play state.
- Membership ID: fresh identifier for each finalized membership.
- Membership record: persisted membership ID, coordinator identity, member identities, and immutable circular turn order.
- Pending enrollment: coordinator's persisted selection list and enrollment identity, not yet playable membership.
- Session ID: runtime epoch separating current traffic from stale traffic.
- State revision: coordinator-issued ordering within a session.
- Active position: index into the saved circular order.

IDs separate messages; they do not authenticate senders. Document the trusted, non-adversarial operating assumption. Validate transport sender identity and membership but do not describe that validation as cryptographic authentication.

## Radio topology and discovery

Use unencrypted ESP-NOW, not Bluetooth. Configure a common region-appropriate radio channel and station interface so unassigned pucks can discover one another. Document the channel and compatibility policy.

Discovery and enrollment use broadcasts plus directed replies/requests. No coordinator is required to send or receive discovery broadcasts. Normal play is a logical star: members send requests to the coordinator, which distributes authoritative state. There is no mesh routing or direct token transfer.

Discovery replies contain coordinator, membership, session, revision, and relevant status. Replies from participants are hints to locate authority, not majority votes that create authority. Several nodes can share stale state. Confirm current playable state with the coordinator. Random IDs are not sortable evidence of which membership is newer.

The enrollment initiator always becomes coordinator. Do not elect a coordinator by MAC ordering or by the most common discovery reply.

## Startup, recovery, and returning coordinators

Every booting device starts with its turn indication off and performs discovery, even if it was previously coordinator.

- Restore saved membership and order, pending enrollment, and any durable finalization recovery state.
- Reconnect to a confirmed current coordinator when possible.
- Adopt current state only if the confirmed membership includes this device. A device excluded from the current game remains unassigned; discovery must not silently enroll it.
- A confirmed replacement membership supersedes a saved coordinator role. A returning old coordinator must relinquish obsolete authority.
- A candidate enrollment announcement alone must not erase committed membership or establish playable authority.
- When all devices restart, the saved coordinator may reconcile its saved members and establish a fresh session. Do not choose a new leader automatically.
- Do not persist every ordinary turn. If active position cannot be established safely, wait for an explicit claim.
- A participant restart may recover a known current turn from the live coordinator without forcing an unnecessary claim.
- An absent old coordinator must never prevent fresh enrollment.

Startup discovery alone is insufficient: periodic synchronization must also handle an old coordinator that remained powered but lost radio contact. Retain sufficient replacement/departure evidence to reconcile old memberships. Define its persistence and lifecycle.

Do not promise instantaneous retirement of unreachable devices. Define bounded expiration of stale active indications, require each participating device to act in at most one membership, and pause on ambiguous authority. Conflicting discovery answers must not trigger an arbitrary winner.

Optional human-readable puck numbers may be assigned over USB and persisted, but never determine membership, order, or leadership.

## Enrollment and saved progress

Turn-order setup creates a fresh membership. No separate pairing, join, leave, undo, or reset gesture is needed.

- Any powered-on node, including an assigned or disconnected node, can initiate enrollment.
- The initiator becomes candidate coordinator and creates fresh enrollment identifiers.
- The initiating gesture does not select that device. It must later be tapped to choose its position.
- Other available nodes, including previously assigned nodes, may be selected by tapping them.
- Each accepted selection is durably saved by the coordinator before selection acknowledgment. An acknowledged selection must survive coordinator restart.
- Repeated selections never duplicate or reorder a node. Briefly acknowledge that it is already selected.
- Selection suspends the selected node's participation in its old game. It does not yet establish a playable new membership. Its old game pauses when that participation is lost.
- Merely hearing enrollment must not erase an unselected node's saved membership.
- Player count derives from selected order, not discovery count.
- Enforce 2–8 players and provide clear capacity feedback.
- Require coordinator inclusion before finalization.

Mistakes are corrected by physically swapping pucks between players or by finalizing and then starting enrollment again. Do not add individual undo, rollback, or a separate in-enrollment restart gesture.

Inactivity must not discard saved selections or restore the previous game. Leave enrollment pending with a slower reminder pattern; later interaction resumes it. Devices may recover pending progress after restart. Document recovery when the candidate coordinator is permanently unavailable: another device can initiate fresh enrollment, superseding abandoned pending participation through explicit selection.

For unexpected overlapping enrollment attempts, detect conflict where observable, suspend selection/finalization with clear feedback, and return to a retryable state. Do not silently pick a coordinator or merge selections. Define how contenders withdraw and can retry without an extra user-facing control; no attempted conflict recovery may undo an already durable finalization decision. Document detection limits during radio partitions.

## Button gestures

Implement a debounced, nonblocking gesture state machine with configurable timing.

Normal mode:

1. A short tap by the active node requests an immediate pass on debounced release, without a double-click delay.
2. An inactive node's ordinary tap does nothing.
3. A hold of at least three seconds produces threshold feedback.
4. Release requests a claim if a valid membership/session allows it. A claim moves the active position to that node; it never rotates or rewrites saved order.
5. A successful long hold never produces a normal tap.
6. Long-hold release opens a configurable 1.5-second window.
7. Two short taps completed in that window initiate enrollment. They do not pass the turn.
8. A lone tap in that window is consumed; expiration returns to ordinary input.

Unassigned/disconnected nodes can perform this same gesture. Their long hold cannot claim without a valid session but still arms enrollment. Accept the side effect that a connected node may claim before the following two taps start enrollment.

Enrollment mode:

- Taps select nodes.
- Only a hold of at least three seconds followed by release on the initiating coordinator requests finalization.
- Holds on other nodes do not finalize or invoke normal claims/nested enrollment gestures.
- The coordinator must already be selected, and at least two nodes must be selected.
- Rejected finalization gives recognizable feedback and preserves pending selections.

Specify debounce, deadline boundaries, held-button startup, clock wraparound, and mode transitions. Avoid blocking delays. Ensure the initiating device's finishing gesture cannot accidentally select itself.

## Finalization and reliable recovery

The coordinator's valid final hold/release is the user's final decision. Persist the finalized membership and recovery record before distributing the decision. No undo or automatic rollback is offered.

Use an explicit durable protocol:

- Distinguish pending selections, readiness/preparation if used, durable final decision, distribution, and playable activation.
- Members acknowledge durable membership adoption only after persistence succeeds.
- Retransmit and reconcile idempotently after lost messages, acknowledgments, or restarts.
- Begin confirmation animation only when all selected nodes have adopted the membership and are ready.
- Until ready, show a recognizable finishing-setup state and automatically retry. Do not require another long hold merely because a packet was lost.
- Do not interpret a timeout after a durable final decision as permission to restore an old membership.
- Do not claim wireless commit or simultaneous activation is atomic. Define failure behavior and observable safety bounds.

If a selected member becomes permanently unavailable, allow fresh enrollment to rebuild the group. A pending finalization must not make the system unrecoverable. Explicitly selected replacement membership must supersede old recovery traffic.

## Turn passing, communication failures, and recovery

Use application request IDs, duplicate suppression, session epochs, state revisions, acknowledgments, bounded retry bursts, and periodic reconciliation.

Pass requests must refer to the expected session and specific turn revision. Multiple taps, duplicate packets, and delayed distinct requests for the same turn must advance at most once, even if the order later cycles back to that player. Define arbitration for simultaneous claims and other competing requests.

Show brief pending-pass feedback. Do not indicate a confirmed new turn before authoritative state is accepted. Lost acknowledgments must resolve through retries/synchronization without requiring players to guess whether to tap again.

A single missed packet should not pause the table. Sustained participant loss pauses instead of silently skipping that player. Define heartbeat, retry, expiration, and recovery thresholds with reasonable radio traffic. Bound stale turn indications and handle any transient overlap explicitly; do not assume distributed lights change atomically.

If the coordinator disappears, pause. Temporary failures recover automatically when reconciled state is certain. Require a claim only when the active position is uncertain. Permanent coordinator loss requires fresh enrollment; do not implement automatic mid-game failover.

Validate message version, size, sender, membership, session, and revision. Handle malformed, stale, duplicated, delayed, and reordered traffic. Specify serialization and payload limits for the actual framework version. Do not serialize raw compiler-dependent structures as the wire format.

Keep radio callbacks short: copy validated bounded input to queues and process in the application loop/task. Define queue overflow behavior and bounded memory use.

## Synchronized animations and single-color feedback

Use a single-light chase, one complete circuit:

- After finalized enrollment: first selected node lights immediately at animation start, then the next node every approximately one second. Each tick turns the previous light off. After the circuit, all flash once, then only the first player stays lit.
- After a successful claim: start at the claiming node and traverse saved circular order at approximately 0.5 seconds per tick. All flash once, then only the claiming node stays lit.

Keep intervals and final-flash duration configurable. Claims never change saved order.

Schedule animations using a unique animation ID, starting position, intervals, and a future coordinator-time start, with a lightweight clock-offset estimate. Do not rely on a fresh radio command for every tick. Define timing tolerance, late arrivals, missed schedules, interruption, and restart behavior. Duplicate or late packets must not restart completed animation.

Consume ordinary taps during animations; do not queue future passes. Preserve the long-hold enrollment escape path through animation/input-mode transitions, including the two taps immediately after a claiming hold. Define precedence so the claim animation cannot swallow that enrollment gesture. Errors interrupt animations and take priority.

Use a compact documented pattern vocabulary for unassigned/disconnected, waiting for claim, enrollment awaiting selection, selection acknowledgment/selected, finishing setup, pending pass, current turn, and paused/error. Reuse patterns only when mode/context makes meaning clear. Single-color LEDs only; no RGB assumptions.

No separate coordinator-identification pattern is required: the initiator is the coordinator. Clearly explain that its initiating gesture does not select it and that finalization happens on that same puck, not the last selected puck.

## Diagnostics

Provide structured snapshots and a bounded event log over USB serial: membership/session/enrollment IDs, coordinator, saved order, active position, revision, pending finalization, last-seen times, retries, persistence errors, and gesture/animation events.

Provide commands for inspection, optional puck-number assignment, and forgetting saved membership. Forgetting must leave the puck capable of enrollment. These maintenance commands are not required for normal table recovery.

Keep diagnostics transport-independent. BLE GATT read-only diagnostics and an iPhone app are future work, not initial deliverables. Phone access must never be required.

## Architecture

Separate hardware/clock adapters, gesture recognition, enrollment and play state machines, wire serialization/validation, coordinator/participant behavior, persistence, animation scheduling, and diagnostics.

Use deterministic core logic shared unchanged between firmware and the host simulator. Inject clocks, radio, storage, and physical inputs/outputs. Avoid a separate simplified simulator implementation of the protocol.

## First-class simulated-table harness

Build a complete multi-device simulator, not only unit tests.

Each simulated puck has independent runtime state, clock, persisted storage, button input, and LED output. Drive actual taps and holds through the real gesture recognizer. Do not bypass user behavior by directly setting membership or active state in end-to-end scenarios.

Simulate:

- Broadcast discovery before any coordinator exists and directed traffic after discovery.
- Packet loss, duplication, delay, reordering, asymmetric links, partitions, and healing.
- Independent clock offsets/drift, timer boundaries, and wraparound.
- Power loss/restart of individual or multiple devices, preserving only durable storage.
- Storage failures and interrupted writes consistent with the chosen persistence adapter's guarantees.
- Failures around each selection/finalization persistence and message boundary, including delivery to only a subset of nodes.
- Radio queue pressure and bounded resource exhaustion.

Use deterministic schedules and seeded randomness. Failure output must include seed, replay command, and a timeline of actions, messages, persistence transitions, roles, and LEDs. Provide a readable virtual-table timeline showing each puck's visible behavior. A graphical UI is optional; a useful text/HTML trace is sufficient.

Testing layers:

1. Focused core/protocol/gesture tests.
2. Scripted full-table scenarios for every supported user flow.
3. Fault-injected variants of those scenarios.
4. Bounded randomized long-running scenarios with reproducible seeds and an optional longer local stress command.

Create a traceability table mapping each supported behavior to automated scenarios and remaining hardware validation. Do not claim exhaustive verification of all possible executions.

Define invariants before implementation, including:

- A request advances its referenced turn at most once.
- A device never actively participates in two memberships.
- Claims preserve saved order.
- Selection acknowledgment follows durable coordinator storage.
- Stale messages cannot restore superseded membership or turn state.
- Finalized enrollment survives restart and resumes incomplete distribution.
- Loss of communication clears stale active indications within the documented bound.
- Animation packets are idempotent and cannot create unintended passes.
- Once communication and storage are healthy, recovery completes within a defined bound or reaches a documented user-action state. Remaining paused forever is not success.

Scope invariants to explicit failure assumptions. Distinguish local authority safety, bounded visual convergence, and liveness; do not assert impossible instantaneous global agreement under arbitrary partitions.

Required scenario coverage:

- All supported player counts, nonconsecutive MAC identities, wraparound.
- First setup with no saved coordinator, all devices booting together, and staggered startup.
- Expansion, reduction, arbitrary reuse of assigned pucks, and coordinator changes.
- Explicit coordinator selection, duplicate selections, capacity, rejected finalization.
- Mistake correction through finalizing and repeating enrollment.
- Persisted enrollment inactivity and recovery; candidate coordinator disappearance.
- Competing enrollment attempts and recovery without extra controls.
- Every gesture boundary and relevant mode transition, especially claim-to-enrollment during animation.
- Claims and simultaneous claims, saved order unchanged.
- Enrollment/claim animations, late packets, input during animation, and error interruption.
- Duplicate/delayed passes, lost acknowledgments, and rapid repeated taps.
- Partial finalization, persistence failures, restart at protocol boundaries, and fresh enrollment superseding stuck recovery.
- Coordinator/participant restarts, old coordinator return, coordinator remaining powered across a partition, conflicting/stale discovery replies.
- Sustained member loss, temporary loss, automatic recovery, and explicit-claim recovery.
- Malformed messages, unsupported protocol versions, queue capacity, and stale sessions.

Simulation does not verify electrical behavior, actual RF reliability/range, physical switch bounce, LED brightness, timing on real hardware, or battery runtime. Clearly separate simulated evidence from hardware evidence.

## Deliverables and completion

Deliver:

1. Compilable firmware for identical images on the three boards.
2. Pinned setup, VS Code configuration, and CI.
3. Automated core/protocol tests and the multi-device simulated-table harness.
4. Scenario traceability, deterministic replay commands, and readable example table traces.
5. README covering macOS setup, flashing, serial configuration, controls, wiring, troubleshooting, and normal recovery without a debug connection.
6. Concise protocol/state-machine documentation, including authority, persistence boundaries, replacement membership, fault assumptions, safety bounds, liveness, and animations.
7. Hardware bring-up checklist: USB serial, button, verified LED driver, two-node communication, three-node enrollment/turn passing/claims/animations, power-loss recovery, then battery and runtime measurements.
8. Short backlog for hardware validation, optional battery sensing, BLE diagnostics, and custom PCB work.

Run available firmware builds, host tests, and simulation suites. Report failures and unsupported checks honestly. Do not call hardware behavior verified based only on compilation or simulation.

Conclude with implementation status, checks run, unresolved issues, and exact steps to take when the boards arrive.
