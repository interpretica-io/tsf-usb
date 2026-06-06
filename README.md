# tsf-usb

Enumerating the USB devices attached to a Test Agent, packaged as an
external Test Environment (TE) repository (consumed with the
`TE_EXT_REPO` builder directive). It reads them over a low-level C
library — **libusb-1.0, no Python, nothing spawned** — for both an
inventory and a security posture.

Three libraries:

- `ta_usb` — agent side. USB enumeration over **libusb-1.0**
  (`libusb.h`, `-lusb-1.0`): list the attached devices with their
  descriptors (VID/PID, device and interface classes, speed, string
  descriptors), and dump one device's full descriptor tree. Read-only —
  it enumerates and reads descriptors, it never configures a device or
  transfers data. The agent and its RPC server both link it.
- `rpcs_usb` — the `usb_*` RPCs for the RPC server of the agent, thin
  wrappers over `ta_usb`. Enumeration happens on the agent, where the
  devices are.
- `tapi_usb` — engine side. `tapi_usb.h` lists devices into a
  `tapi_usb_device` vector and dumps one device's descriptors;
  `tapi_usb_audit.h` reads the inventory as a security posture through
  tsf-cybersec; `tapi_usb_rpc.h` is the one-per-RPC layer beneath.

TE has no USB enumeration of its own.

## What it reads

```c
te_vec devices = TE_VEC_INIT(tapi_usb_device);
const tapi_usb_device *dev;

CHECK_RC(tapi_usb_list(rpcs, &devices));
TE_VEC_FOREACH(&devices, dev)
    RING("%04x:%04x %s %s", dev->vid, dev->pid,
         tapi_usb_class2str(dev->dev_class),
         dev->product != NULL ? dev->product : "");
tapi_usb_list_free(&devices);
```

Each device carries its bus/address/port, vendor and product IDs, the
device class triple and `bcdUSB`, the libusb speed, and the interface
classes of its active configuration — plus the manufacturer, product
and serial strings when the device could be opened to read them
(reading strings opens the device, which may need privilege; the
numeric descriptors come back regardless). `tapi_usb_device_dump()`
reads one device's whole config/interface/endpoint tree as text.

## The library is linked, not a program

`ta_usb` does not run `lsusb` and parse its output. It links libusb and
calls `libusb_get_device_list()` and the descriptor getters in the
agent's RPC server process, reading the descriptors as libusb's own
structures. Across the RPC a device comes back as a tab-separated
record, which the engine side parses into `tapi_usb_device`.

## Security posture

`tapi_usb_audit()` reads the inventory and reports through
tsf-cybersec's finding model. It only reads what is enumerated — it
changes nothing and talks to no device — but the findings are about
physical-port control, so point it at a host, kiosk or device you own
or are **authorized** to assess.

| Finding | Severity | Raised when |
|---|---|---|
| `usb.device-present` | info | a device is attached (one per device) |
| `usb.input-and-storage` | high | one device is both HID and mass-storage (the BadUSB composite shape) |
| `usb.mass-storage` | medium | storage is attached and the policy forbids it |
| `usb.hid-input` | low | an input device is attached and the policy forbids it |
| `usb.app-specific-interface` | medium | an application-specific interface (class `0xfe`, often DFU/firmware update) is exposed |
| `usb.vendor-specific` | low | a vendor-specific (opaque) interface is exposed |
| `usb.unexpected-device` | medium | the device is not on the policy's `VID:PID` allowlist |
| `usb.none` | info | nothing is attached (or nothing could be enumerated) |

A finding's subject is the device's `VID:PID` (stable between runs); the
bus/address stay in the log detail, so a verdict matches cleanly in TRC.

## Agent host requirements

- **libusb-1.0** with its development headers (Debian:
  `apt install libusb-1.0-0-dev`). The enumeration API used here
  (`libusb_get_device_list`, `libusb_get_active_config_descriptor`,
  `libusb_get_string_descriptor_ascii`) is the long-standing 1.0 API.
- Reading **string descriptors** opens each device; on Linux that
  wants access to the device node (root, or a `udev`/`plugdev` rule).
  Without it, the numeric descriptors and interface classes — which is
  what the posture is built from — still come back.

## Usage

Declare the repository in an external libraries catalog and pass it to
`dispatcher.sh --external=<catalog.yml>`:

```yaml
repositories:
  - name: tsf_usb
    url: https://github.com/interpretica-io/tsf-usb.git
    ref: <tag>
    libs:
      - ta_usb
      - rpcs_usb
      - tapi_usb
```

In `builder.conf`, bind `tapi_usb` to the engine, list `ta_usb` and
`rpcs_usb` among the RPC server's libraries, and add the RPC
definitions to both platforms:

```
TE_EXT_REPO_USE([tsf_usb], [ta_usb rpcs_usb], [tapi_usb])

TE_LIB_PARMS([rpcxdr], [${TE_HOST}], [],
             [--with-rpcdefs=tarpc_job.x.m4,../ta_usb/usb_rpc.x.m4])
TE_LIB_PARMS([rpcxdr], [${TE_TA_TYPE}], [],
             [--with-rpcdefs=tarpc_job.x.m4,../ta_usb/usb_rpc.x.m4])
```

`tapi_usb_audit` reports through tsf-cybersec, so that repository (and
its own prerequisites, tsf-kernel and tsf-devtool) must be built too.
The RPC program number is **26** (20–25 are taken by the other tsf
agent RPCs); change it in `usb_rpc.x.m4` if it ever collides.

## What was verified, and what was not

**The C was not compiled here** — no TE toolchain. `ta_usb.c`,
`rpcs_usb.c`, `tapi_usb*.c` and the TE integration (the three
`meson.build`s, `usb_rpc.x.m4`, `TE_EXT_REPO` wiring) were written to
the tsf-upnp template but not built.

**The libusb usage was checked against the real library.** Every
libusb-1.0 call, struct field and enum `ta_usb.c` uses — the device
list, the device/config/interface/endpoint descriptors, the bus/
address/port/speed getters, `libusb_get_string_descriptor_ascii`,
`libusb_error_name` — was compiled against the installed libusb-1.0
(1.0.29) headers (`-fsyntax-only`) and type-checks. What was **not**
exercised: a live enumeration with devices attached, the string-
descriptor permission path, and the RPC marshalling. The first suite to
build tsf-usb should expect the ordinary first-build fixes.

## Scope

- **Read-only.** tsf-usb enumerates and reads descriptors. It does not
  claim an interface, set a configuration, transfer data or touch
  firmware; `usb.app-specific-interface` reports that a DFU-style
  interface *exists*, it does not enter it.
- **Posture is about physical control.** The findings mean something
  only where the USB ports are meant to be controlled (a kiosk, an
  appliance, a locked-down host); on an open workstation a keyboard and
  a disk are expected.
