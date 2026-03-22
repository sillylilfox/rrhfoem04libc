# rrhfoem04lib

A lightweight C library for the **RapidRadio RRHFOEM04** USB/Ethernet RFID reader, targeting embedded systems and native applications.

## Why does this exist?

There is already a Python library available for this reader(Big thanks to its developer!), which works great for desktop applications. However, Python is not always available. Embedded Linux boards, bare-metal systems, and resource-constrained environments need something they can compile and link directly.

## What it supports

- **ISO 15693** — inventory, read/write/lock blocks, AFI/DSFID, EAS flag, passwords, page protection, privacy mode, destroy, NXP extensions (signature, system info, Stay Quiet Persistent)
- **ISO 14443A** — request, anti-collision, select, halt, Mifare Classic (authenticate/read/write), Mifare Ultralight
- **RFID system commands** — RF power control, low/normal power mode, RF on/off
- **Reader commands** — buzzer, relay control, firmware info, device reset
- **Ethernet configuration** — IP, gateway, DNS, MAC, ports, NBIOS name (Ethernet kit only)

## Dependencies

- [hidapi](https://github.com/libusb/hidapi) — for USB HID communication

## Building

You can build it with GCC or MinGW. Other compilers are not tested yet.
Also, this could be compiled as a static library, shared library or you can just embed it into your code.

## Quick example

```c
#include <stdio.h>
#include "rrhfoem04lib.h"

int main(void) {
    hid_device *reader = initrrhfoem04(true, 0, 0);
    if (!reader) return 1;

    u8 uid[RRHF_UID_LEN];
    if (ISO15693SingleSlotInventory(reader, uid) > 0) {
        printf("UID: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
               uid[0], uid[1], uid[2], uid[3],
               uid[4], uid[5], uid[6], uid[7]);
        BuzzerBeep(reader, 1, 0);
    }

    killrrhfoem04(reader);
    return 0;
}
```
