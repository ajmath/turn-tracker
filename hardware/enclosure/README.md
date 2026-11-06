# Turn-tracker enclosures

Prototype circular enclosures with centered illuminated buttons, rounded top rims,
recessed USB access, removable bases, and slide-in ESP32-C3 mounts.

| | Option 1 | Option 2 |
|---|---:|---:|
| Outside diameter | 90 mm | 70 mm |
| Height, excluding button cap | 45 mm | 38 mm |
| Nominal button panel hole | 28.5 mm | 12 mm |
| Modeled hole with print allowance | 28.8 mm | 12.3 mm |
| Button depth below panel | 33.3 mm | 17 mm |
| Battery orientation | On 42 × 10 mm edge | Flat on 42 × 25 mm face |

Walls, roof and base are 2.4 mm thick. The outer top rim has a 2.4 mm round-over.
Dimensions are millimeters throughout. The user successfully test-printed the
previous bar. This reinforced revision and actual component fits remain unverified;
the components have arrived and the first fit adjustments are incorporated below.

## Files and Tinkercad

The ZIP bundles the same STL files found in `stl/`, together with source,
references, previews, instructions and validation results.

- Import `stl/option-1-exploded.stl` or `stl/option-2-exploded.stl` into Tinkercad
  at **100% scale, millimeters** to view separated parts on one workplane.
- Each option also has individual `shell`, `base` and `brace-bar` STLs.
- `component-layout-REFERENCE` STLs show component placement. Do not print them
  as enclosures; they contain component solids.
- `preview.png` shows both layouts; `interface-details.png` shows the mount and
  USB interface; `brace-bar-detail.png` shows the bar's end section and PCB edge.
- `puck.scad` and the option wrappers are generated CAD sources. Keep their
  `reference/` folder alongside them when opening them in OpenSCAD.

## Printing and assembly

Shells are exported roof-down, bases flat, and bars standing on one end. For the
bar, start with a 5–8 mm brim, 0.16–0.2 mm layers and no supports. Its constant
section avoids overhangs in that orientation. Print a bar/slot fit sample before
a full enclosure; printer tolerances can change the sliding fit.

### ESP mount and retaining bar

The PCB underside sits 5.4 mm above the external bottom. The two rails provide
18.58 mm clear width and a 1.5 mm high edge channel. Slide the board in from the
center of the case, with the USB connector toward the side opening. The PCB's
21 mm direction points toward USB. The mount assumes clear edges and no headers.

The bar retains its original exterior profile and 30 mm length. Its PCB notch
is 1.5 mm high. The base side channels are tightened by 0.1 mm on each of
their four sides compared with the previous print, reducing the channel width
from 18.78 to 18.58 mm and height from 1.7 to 1.5 mm. The bar notch also tightens
by 0.1 mm above and below the PCB. The front stop retains its extra 0.2 mm
clearance. Verify the revised fit on the first print.

The socket roof is retained over the **sloped section**, which provides retention.
Only the flat rear roof is removed. Rear walls remain 1.8 mm thick, built outward.
The bar-to-socket sliding clearance stays 0.2 mm. The generation checks explicitly
verify that the sloped roof exists and the flat roof stays open.

Outside diameters stay 90 / 70 mm and both shell files are unchanged. Option 2
battery placement remains unqualified at the user's request. Any intersections
with its illustrative battery are reported in `validation.json`.

Install the board first, then slide the complete rigid bar laterally through
both sockets. Pull or push the bar sideways to remove it. It has no flexible
snap head or positive axial latch. Attach the battery after inserting the board
and bar; the battery can obstruct their insertion paths. Fit wiring and antenna
cable before closing the shell.

### Battery

Option 1 has a 44 mm wide, 2 mm thick tape panel leaning 8° away from the center,
with two rear triangular braces. The space under the projecting shelf is
filled solid across its full 44 mm width, down to the base; no individual shelf
ribs are used. A bottom shelf supports the battery's 10 mm
edge, and a 2.5 mm front lip prevents slipping. Seat the battery on the shelf,
then press it against tape on the sloped face. Gravity bears on the shelf and
into the panel. The reference placement allows 0.3 mm tape thickness.

Option 2 leaves a flat interior area for double-sided tape beneath the button.
Battery guides and strap slots are not fitted.

### USB, switch and shell

USB access is 9.6 × 4.8 mm with 1.8 mm corner radii, centered 7.51 mm above the
external bottom. Its 11.6 × 6.8 mm recess has 2.8 mm radii, leaving a 1 mm border,
and is 2.1 mm deep at its center. A base upstand and the shell meet around the
socket with 0.1 mm nominal assembly clearance. The metal cable plug enters the
opening; check the cable housing stays clear outside it.

The slide switch glues directly to the interior base between two L guides with
inner faces 10.6 mm apart, increased by 0.2 mm from the original 10.4 mm gap. Both legs of each L are 3 mm thick, tripled from
1 mm. The rear pocket face is relieved 0.2 mm for fit. Long arms are shortened
from 8.6 to 7.6 mm; both brackets move 1 mm radially toward the edge. Guides and shell cutout
are 5.6 mm high. The bracket
position is now inset 2 mm from the original layout. Verify actual flange, terminal
and actuator geometry, especially actuator reach through the shell.

Use three M3 × 8 mm Phillips #1 flat-head machine screws per enclosure
(DIN 965A, 90° countersunk, 0.5 mm thread pitch). The 8 mm length includes
the head. Tap the printed 2.5 mm shell pilots with an M3 × 0.5 tap. Both
bases have 3.2 mm through holes with 7.0 mm diameter, 90° underside
countersinks so the heads can sit below the bottom surface. Check seating
on the first print.

## Reference geometry and validation

PCB and USB geometry were extracted from the supplied Seeed Studio XIAO-ESP32-C3
STEP model using OpenCascade. Measured model bounds are 20.955 × 17.78 × 1 mm for
the PCB and 7.3 × 8.94 × 4.2 mm for the connector, which projects about 1.508 mm
beyond the PCB. These are model dimensions, not manufacturing tolerances.

`reference/board.stl` contains the normalized board solids and `reference/usb.stl`
the connector. PCB underside is Z=0, PCB front is Y=0, and USB points toward +Y.
`reference/measurements.json` records the source bounds and coordinate mapping.

The generator checks positive-volume collisions between modeled parts/components,
constant bar section area, and closed meshes with consistent winding. It samples
97 board insertion positions and 73 complete rigid-bar insertion positions per
option, with shell and battery absent. Option 2 battery intersections are explicitly
excluded while its placement is deferred; all other modeled collision checks remain
enforced. Results and mesh bounds are recorded in
`validation.json`; printable STL files are also reloaded to check export integrity.

These checks do not establish physical fit, structural strength, cable fit or
printer tolerances. Button body/nut/bezel envelopes, LED driver, antenna cable,
soldered wires, and switch flange/terminals remain incompletely modeled. The
button body currently uses its panel-hole diameter as a provisional envelope.
The earlier bar was test-printed successfully by the user; that does not qualify
this thicker revision or component fit. No slicer verification or Tinkercad import
has been performed here.

## Regeneration

`generate.py` is the source of dimensions and emits equivalent SCAD and STL CSG
geometry. Open `option-1.scad` or `option-2.scad` for viewing; supported `part`
values are `assembly`, `layout`, `shell`, `base`, and `brace-bar`. Change dimensions
in `design()` and regenerate to keep sources and exports synchronized. OpenSCAD
itself is not required for generation and has not been executed here.

Use an isolated environment; dependencies are separate from firmware tooling:

```sh
python3 -m venv /tmp/turn-tracker-cad
/tmp/turn-tracker-cad/bin/python -m pip install -r hardware/enclosure/requirements-cad.txt
MPLCONFIGDIR=/tmp/turn-tracker-mpl /tmp/turn-tracker-cad/bin/python hardware/enclosure/generate.py
```

Generation refreshes SCAD, STL, previews, validation and `tinkercad-enclosures.zip`.
The supplied normalized reference meshes are inputs; regenerating enclosures does
not require OpenCascade or access to the original Downloads folder.
