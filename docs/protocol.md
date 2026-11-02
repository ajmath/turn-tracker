# Protocol, state machines, and failure contract

## Authority and invariants

Non-adversarial, trusted pucks only. Transport MAC validation and IDs are **not
cryptographic authentication**. A device's station MAC identifies it. An ESP-NOW
peer cache entry is unrelated to membership. The enrollment initiator becomes
coordinator; no MAC ordering, voting, failover, or most-common discovery answer
creates authority. One intended game and enrollment process are assumed in range.

The implementation is organized around these target invariants:

| Property | Mechanism / scope |
| --- | --- |
| A request advances its referenced turn at most once | Coordinator serializes requests, requires exact session and revision; successful pass/claim increments revision |
| One active membership per device | One committed record, durable reservation suspends old participation, explicit selection required for different adoption |
| Claims preserve saved order | Active index changes; committed order never changes within a membership |
| Selection acknowledgment follows storage | Candidate persists appended order before sending Selected; storage failure sends no success acknowledgment |
| Stale traffic cannot reinstate an old game | Different Commit requires current explicit reservation; old State cannot overwrite saved membership; retired roles persist |
| Final decision survives restart | Committed record plus deciding flag persist together before Commit distribution; retries after reboot |
| Stale steady turn indication expires | Three-second lease, paused/error patterns take priority; includes explicit bounded network-delay assumption below |
| Animation duplication cannot pass | Scheduled timeline is local, unique per session; completed IDs do not restart; ordinary taps consumed |
| Healthy recovery terminates | Healthy live authority restores known turn; coordinator reboot reaches explicit waiting-for-claim; missing authority requires fresh enrollment |

These are implementation targets with tested examples, not a proof over all
possible executions. See coverage gaps before using this as a production protocol.

## Wire and adapters

Protocol version 1, fixed **117 bytes**, no compiler-layout serialization. All
integers are little endian. Offsets:

| Offset | Field |
| --- | --- |
| 0..3 | TT magic, version, message kind |
| 4..17 | sender MAC, boot epoch u32, sequence u32 |
| 18..25 | enrollment ID u64 |
| 26..88 | membership ID u64, coordinator MAC, count u8, eight MAC slots |
| 89..104 | session, revision, request, coordinator/local timestamp (u32 each) |
| 105..116 | animation ID u32, start u32, interval u16, active u8, flags u8 |

Kinds: Discover, Presence, Invite, Select, Selected, Commit, Adopted, State, Sync,
Pass, Claim, Conflict, Departed. Empty membership records are permitted for
non-play messages; nonempty records must have 2–8 unique unicast members and
include the coordinator. Active index must be in range or 255 (unknown). Interval
must be zero or 100–2000 ms. Known IDs with altered membership contents are rejected.

Firmware callbacks check bounded length/header and enqueue a fixed-size copy.
16 receive slots; excess drops are counted. Loop drains at most 16 frames per
iteration. Parsing and all storage/protocol processing happen outside callbacks.
Peer diagnostics have 8 slots; expired entries can be reused. Firmware peer cache
can evict unicast entries when full and retains broadcast. Target payload limit is
250 bytes, unencrypted total peer limit 20; see versions.md for actual SDK checks.

Firmware uses station interface, always-listening Wi-Fi, channel 1 (build flag
`RADIO_CHANNEL` can select another supported common channel). All images in
a table must use the same channel/version. Channel 1 is the initial common-channel
choice; verify local radio rules for deployment. No router association, BLE,
encryption, or radio sleep. Do not mix protocol versions within a game.

Hardware adapters are in `src/main.cpp`; pure C++ core is `include/turn/`.
Clock values, physical button input, radio sends/receives, and Storage are injected.
Firmware and simulator compile the same Node, Button, serialization, and storage
codec. Simulator has independent clocks, disks, queues, button states, and outputs.

## Durable boundaries

A versioned checksummed **246-byte** blob stores committed membership, candidate's
pending ordered selection list, reservation/candidate identity, final decision flag,
retired-role flag, and optional human puck number. No ordinary turn is persisted.
The NVS adapter uses one Preferences blob write and checks success before updating
RAM. Actual pinned Preferences calls `nvs_commit` in `putBytes`.

[NVS robustness contract](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32c3/api-reference/storage/nvs_flash.html)
allows the in-progress new value to be lost at power failure. Tests model failed
writes preserving the old record; power-boundary tests reboot from durable state.
Hardware power-cut tests remain required, including ambiguous API failure after a
physical commit. A bad checksum is reported on USB and does not create authority.

1. Initiation persists a fresh pending enrollment; it selects nobody. Any prior
   coordinator role is durably retired. Hearing Invite alone changes no saved record.
2. Selecting participant persists reservation/suspension before repeated Select.
3. Candidate appends unique node and persists before Selected. Duplicates only ACK.
4. Valid final hold persists pending list as committed membership plus deciding=true.
   This is the final user decision, with no rollback or timeout undo.
5. Commit retries. A selected member persists adoption before Adopted. New adoption
   requires its matching reservation and candidate MAC; old recovery cannot replace
   a newer explicitly selected reservation.
6. Candidate collects session-scoped adoption/sync responses from all selected
   members. It persists deciding=false, then schedules confirmation and active 0.
   Lost schedules converge through periodic State; no extra hold is needed.

An absent member after decision leaves finishing setup. New explicit enrollment can
replace it. Pending selections do not expire. A missing candidate's suspension
also persists; when its invitations stop, the normal escape gesture becomes
available, or a new candidate's invitation can be explicitly selected.

## Discovery, restart, departure

Every boot starts without playable authority and broadcasts discovery. Presence
contains saved membership, coordinator, enrollment, session, revision, and status;
it is only a location hint. Random IDs are equality tokens, never recency ordering.
Fresh invitation-query responses may retire an abandoned **pending** process when
a durably selected member confirms it has explicitly moved elsewhere. Query tokens
include candidate boot identity and a selection barrier, so old discovery replies
cannot withdraw a selection. They never create playable authority. A participant
confirms its saved coordinator with a directed Sync challenge and
State response; new session acceptance requires the echoed current challenge.
No excluded device adopts current membership merely through discovery.

A saved coordinator waits 3.5 seconds after boot and establishes a fresh boot
session, with active position unknown. Its members must respond in that session;
then an explicit claim establishes a turn. A participant reboot can recover a live
coordinator's known turn. No replacement leader is elected when the coordinator is
missing. Boot epochs are random 32-bit values, enrollment IDs combine boot and local
serial; negligible-ID-collision and non-adversarial-source assumptions are explicit.

A selected node suspends its old game immediately after local persistence. When an
old coordinator advertises state, a former/suspended member sends Departed tagged
with that old membership/session. The coordinator durably retires its obsolete role.
It cannot resume after reboot or renew old turn authority. If that evidence is
unreachable, missing session-scoped member responses still expire its authority.
Departure evidence is retained by the node's current committed record/reservation
and by the old coordinator's durable retired bit. Fresh explicit selection and
adoption replace that local record; no unbounded tombstone history is required.
Only an explicit reservation can authorize any different incoming membership.
A returning abandoned candidate queries its saved selected members through Invite;
fresh replies showing replacement reservation/membership retire its pending process.
No durable final decision is undone by that path. This avoids an indefinitely
returning pending process repeatedly advertising itself after its members moved on.

Unexpected observed competing invitations stop selection/finalization. Candidates
withdraw after three seconds of quiet, retain old-role retirement, and can retry
with the same normal gesture. Observers report conflict to both candidates. A
committed final decision is not withdrawn by Conflict. Detection cannot cover
radio partitions; safe operation assumes one intended process. Simultaneous
partitioned final decisions and fully general ambiguous-authority handling remain
an explicitly unqualified edge case (coverage.md).

## Requests and communication

Heartbeat/reconciliation every 500 ms; discovery roughly every 2–2.5 seconds with
boot-based staggering. Request bursts retry at 200 ms up to five times, then at
500 ms through synchronization until resolved or communication expires. Requests
carry request ID, expected membership/session, and exact turn revision. A pass
also requires that sender to be active. Coordinator handles competing requests in
receive order; first valid revision wins, later competing requests get current State.
Multiple delayed distinct requests for an old turn cannot pass a later circuit.

Three seconds without current member responses pauses the coordinator; three
seconds without valid authority pauses a participant. One lost packet does not
pause. Temporary loss with a known active position resumes automatically. Coordinator
reboot loses active certainty and ends in waiting-for-claim rather than waiting
forever. State changes in play/pause increment revision to order delayed flags.

For the intended timing bound, assume loop service <=20 ms, healthy one-way delivery
<=250 ms, clock drift <=100 ppm, no epoch collision, and no endless replay of old
frames. Stale **steady** active indication clears within approximately 3.6 seconds
of physical link loss (lease + maximum in-flight/processing/drift allowance).
Under stable communication/storage, periodic request/response normally reconciles
within 2 seconds; startup adds 3.5 seconds; activation adds its 1.5-second lead and
animation duration. Use a conservative 15-second healthy convergence test deadline.
Arbitrary delay/partitions cannot guarantee instantaneous global agreement or a
finite recovery bound. Pulse patterns are status feedback, not active-turn grants.

## Gestures and animation

Debounce 30 ms; hold threshold 3000 ms; escape window 1500 ms. All configurable
through Button Timing. Threshold/deadline comparisons include equality; the escape
window is half-open and counts completed debounced short releases. A press begun
inside it but released after expiration is consumed. A held button at boot must
first release before input is armed. Unsigned elapsed comparisons support millis
wrap with all intervals below 2^31 ms and regular loop service. Mode changes consume
in-progress presses where explicit transition is required. Finishing holds never
implicitly select the coordinator.

Claim release preserves the two-tap escape across animation mode. Ordinary animation
taps are consumed; enrollment escape and errors have priority. No future pass is
queued. Error patterns interrupt the visible chase.

Animation ID, saved-order start index, interval, and future coordinator-time start
are transmitted in State. Default enrollment tick 1000 ms, claim 500 ms, lead 1500 ms,
final all-flash 200 ms. Node AnimationTiming config exposes these values; all devices
must share timing config. Each node estimates offset from coordinator timestamp
on received State (one-way estimate, biased by packet delay). Target tolerance is
250 ms under the delay assumption, not precise clock synchronization. Late arrivals
render the appropriate current tick, miss elapsed ticks, and never replay a finished
circuit. A completed animation ID stays completed even if later offset samples move
backward. Duplicate schedules do not restart. A reboot requires synchronization;
coordinator reboot does not replay old animation or restore old active certainty.

## Diagnostic codes

Snapshot status numbers: 0 discovering, 1 unassigned, 2 enrollment, 3 selected,
4 finishing, 5 waiting for claim, 6 playing, 7 paused, 8 conflict, 9 storage error.
Event codes: 1 gesture (Gesture enum value), 2 initiation, 3 selection ACK/count,
4 final decision/adoption, 5 activation, 6 accepted turn revision, 7 animation,
8 rejected selection/finalization, 9 persistence failure, 10 retirement,
11 request retry. Logs are bounded to 64 events; lifetime counters are separate.

State flags: bit 0 playable, bit 1 finishing durable distribution. They are
mutually exclusive. Participants retain finishing feedback until activation.
