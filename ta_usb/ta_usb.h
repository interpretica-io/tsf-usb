/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Agent-side USB enumeration
 *
 * Reading the USB devices attached to an agent over **libusb-1.0**
 * (@c libusb.h, @c -lusb-1.0): the library is linked into the agent and
 * called in-process, nothing is spawned and @c lsusb is not scraped.
 * The agent and its RPC server both link this; the RPCs (see
 * usb_rpc.x.m4) are thin wrappers over these functions.
 *
 * This is read-only. It enumerates devices and reads their descriptors
 * - no configuration is set, no data is transferred, no firmware is
 * touched. Reading the string descriptors opens the device, which may
 * need privilege; when it cannot, the numeric descriptors still come
 * back and the strings are left empty.
 *
 * Results come back as newline-separated text, one record per line with
 * tab-separated fields, the same shape tsf-upnp uses - the engine side
 * parses them.
 */

#ifndef __TA_USB_H__
#define __TA_USB_H__

#include "te_errno.h"
#include "te_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * List the USB devices on the agent.
 *
 * One device per line, tab-separated:
 * @c "bus\\taddr\\tport\\tVID\\tPID\\tclass\\tsubclass\\tprotocol\\t
 * bcdUSB\\tspeed\\tnconf\\tifclasses\\tmanufacturer\\tproduct\\tserial",
 * where @c VID and @c PID are four hex digits, the class triple and
 * @c bcdUSB are hex, @a speed is a #libusb speed code, and @a ifclasses
 * is a comma-separated list of the active configuration's interface
 * classes in hex (e.g. @c "03,08"). The three string fields are empty
 * when the device could not be opened to read them.
 *
 * @param[out] count    Number of devices.
 * @param[out] result   The device lines.
 *
 * @return Status code.
 */
extern te_errno ta_usb_list(int *count, te_string *result);

/**
 * Read one device's descriptors in full, by bus and address.
 *
 * A human- and machine-readable dump: a @c device line, then one
 * @c config / @c interface / @c endpoint line per descriptor, each
 * tab-separated and tagged by its first field. Addresses a device by
 * the @a bus and @a addr from ta_usb_list().
 *
 * @param[in]  bus      Bus number.
 * @param[in]  addr     Device address on the bus.
 * @param[out] result   The descriptor dump.
 *
 * @return Status code.
 * @retval TE_ENODEV    No device at that bus/address.
 */
extern te_errno ta_usb_device(int bus, int addr, te_string *result);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TA_USB_H__ */
