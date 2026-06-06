/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Enumerating USB devices on an agent from a test
 *
 * @defgroup tapi_usb USB devices (tapi_usb)
 * @{
 *
 * Reading the USB devices attached to a Test Agent, over libusb-1.0 in
 * the agent's RPC server (not by scraping @c lsusb): what is plugged
 * in, what each device claims to be, and what interface classes it
 * exposes. Read-only - it enumerates and reads descriptors, it does
 * not configure a device or transfer data.
 *
 * - tapi_usb_list() takes a snapshot of every device into a vector of
 *   #tapi_usb_device;
 * - tapi_usb_device_dump() reads one device's full descriptor tree as
 *   text, by the bus/address from the snapshot;
 * - @ref tapi_usb_audit (tapi_usb_audit.h) reads the snapshot as a
 *   security posture through tsf-cybersec.
 *
 * @code
 * te_vec devices = TE_VEC_INIT(tapi_usb_device);
 * const tapi_usb_device *dev;
 *
 * CHECK_RC(tapi_usb_list(rpcs, &devices));
 * TE_VEC_FOREACH(&devices, dev)
 *     RING("%04x:%04x %s %s", dev->vid, dev->pid,
 *          tapi_usb_class2str(dev->dev_class),
 *          dev->product != NULL ? dev->product : "");
 * tapi_usb_list_free(&devices);
 * @endcode
 */

#ifndef __TAPI_USB_H__
#define __TAPI_USB_H__

#include <stdint.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "te_vector.h"
#include "rcf_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One USB device as enumerated on the agent. */
typedef struct tapi_usb_device {
    /** Bus number. */
    int bus;
    /** Device address on the bus. */
    int addr;
    /** Port number on its hub, or @c 0 when libusb could not say. */
    int port;
    /** Vendor ID. */
    uint16_t vid;
    /** Product ID. */
    uint16_t pid;
    /** @c bDeviceClass (0 means "defined per interface"). */
    uint8_t dev_class;
    /** @c bDeviceSubClass. */
    uint8_t dev_subclass;
    /** @c bDeviceProtocol. */
    uint8_t dev_protocol;
    /** @c bcdUSB, e.g. @c 0x0200 for USB 2.0. */
    uint16_t bcd_usb;
    /** libusb speed code (@c LIBUSB_SPEED_*). */
    int speed;
    /** Number of configurations. */
    int n_configs;
    /** The active configuration's interface classes. */
    uint8_t *iface_classes;
    /** How many are in @a iface_classes. */
    size_t n_iface_classes;
    /** Manufacturer string, or @c NULL (device could not be opened). */
    char *manufacturer;
    /** Product string, or @c NULL. */
    char *product;
    /** Serial-number string, or @c NULL. */
    char *serial;
} tapi_usb_device;

/**
 * Snapshot the agent's USB devices.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[out] devices  Vector of #tapi_usb_device to fill; release with
 *                      tapi_usb_list_free().
 *
 * @return Status code.
 */
extern te_errno tapi_usb_list(rcf_rpc_server *rpcs, te_vec *devices);

/**
 * Read one device's full descriptor tree as text.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  bus      Bus number (from a listed device).
 * @param[in]  addr     Device address (from a listed device).
 * @param[out] dump     String to append the descriptor lines to.
 *
 * @return Status code.
 * @retval TE_ENODEV    No device at that bus/address.
 */
extern te_errno tapi_usb_device_dump(rcf_rpc_server *rpcs, int bus, int addr,
                                     te_string *dump);

/**
 * Does a device expose an interface of a given class?
 *
 * @param device        Device.
 * @param usb_class     A USB class code (e.g. @c 0x03 for HID).
 *
 * @return @c true when the active configuration has such an interface.
 */
extern bool tapi_usb_has_class(const tapi_usb_device *device,
                               uint8_t usb_class);

/**
 * Find the first device with a given vendor and product ID.
 *
 * @param devices       A snapshot from tapi_usb_list().
 * @param vid           Vendor ID.
 * @param pid           Product ID.
 *
 * @return The device, or @c NULL when none matches. Owned by @p devices.
 */
extern const tapi_usb_device *tapi_usb_find(const te_vec *devices,
                                            uint16_t vid, uint16_t pid);

/**
 * Spell out a USB class code.
 *
 * @param usb_class     A USB class code.
 *
 * @return A static string, never @c NULL.
 */
extern const char *tapi_usb_class2str(uint8_t usb_class);

/**
 * Write one device into the log.
 *
 * @param device        Device.
 */
extern void tapi_usb_device_log(const tapi_usb_device *device);

/**
 * Release a device snapshot.
 *
 * @param devices       Vector from tapi_usb_list().
 */
extern void tapi_usb_list_free(te_vec *devices);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TAPI_USB_H__ */

/**@} <!-- END tapi_usb --> */
