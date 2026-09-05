# OSD setup

The PX4 Vehicle Setup **OSD** page appears when component metadata advertises
OSD capabilities. It uses QGC's existing vehicle connection, FTP manager, and
parameter Facts. No connection to the OSD board is opened.

The protocol is the proposed `COMP_METADATA_TYPE_OSD = 6` from
`ark-osd/docs/mavlink_osd/ark.xml`. Type 6 currently lives in the JSON metadata
manifest; no new MAVLink message or generated dialect change is required.
`CompInfoOsd` participates in QGC's existing metadata download, XZ decompression,
and CRC-keyed cache flow. Unsupported firmware skips this metadata type.

## Editing and saving

- Select a display and canvas; add catalog elements and drag them on the canvas.
- Grid displays snap to cells. Pixel displays offer optional snapping.
- Arrow keys nudge, H toggles visibility, and Delete removes the selected element.
- The inspector edits position, visibility, blink, variants, labels, and value bindings.
- Revert restores the last downloaded/saved layout; Defaults loads compiled defaults.
- Import and Export move layout JSON between the editor and local files.
- Save uploads the layout, creating the immediate destination directory if needed,
  then increments the advertised reload Fact and waits for its vehicle acknowledgement.
- Firmware uploads are offered only by displays advertising that feature.

A missing remote layout uses compiled defaults. Other transfer failures do not.
Invalid downloaded layouts remain visible for correction and cannot be saved.
Edits are retained when upload or reload acknowledgement fails. The vehicle owns
transfers, so closing the page does not redirect or destroy an ongoing transfer.
The controller prevents writes while armed.

The preview shows positions and catalog footprints, with labels identifying the
items. It does not use the ARK firmware renderer or display live telemetry.
The drivers provide no applied-layout result or board update progress; a successful
save means the file was uploaded and the reload parameter was acknowledged.

## Sources and compatibility

The schema snapshot and mock fixture follow `ark-osd`'s OSD design and the local
PX4 `dakejahl/ark-osd` branch, inspected on 2026-09-04. The mock metadata was generated
from the three PX4 OSD drivers' `module.yaml` files using
`Tools/module_config/generate_osd_metadata.py`.
The external layout schema reference is embedded in `osd.schema.json` so QGC's
resource-only schema loader can resolve it without network access.

In addition to the schemas, validation checks driver limits: 48 layout entries,
32 visible pixel elements, 12 KiB layout files, and unique grid element IDs.
Layout and firmware destinations must be direct children of `/fs/microsd/osd/`.
Unknown interchange data is retained when editing a downloaded document.

## Hardware-free verification

A Debug PX4 MockLink advertises the OSD catalog, serves compressed metadata, and
retains uploaded layouts for readback. Start a PX4 mock vehicle from QGC's Mock Link
settings, then open Vehicle Setup > OSD. The three display entries include a disabled
MSP display, which remains editable.

```sh
just build
ctest --test-dir build --output-on-failure -R 'Osd(Layout|Controller)Test'
```

`OsdLayoutTest` runs under the CI Unit label. `OsdControllerTest` uses VehicleTest
and MockLink for metadata discovery, FTP save/readback, reload, failure paths,
armed guards, and real QML editor controls. It never opens a hardware link.
