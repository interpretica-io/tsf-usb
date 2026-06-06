/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief What the attached USB devices are worth as a security posture
 *
 * @defgroup tapi_usb_audit USB security posture
 * @ingroup tapi_usb
 * @{
 *
 * The agent's attached USB devices read as a security posture and
 * reported through tsf-cybersec: what is plugged in, and whether any of
 * it is the shape of a problem - a single device that is both a
 * keyboard and a mass-storage disk (the BadUSB pattern), a device
 * exposing a firmware-update or vendor-opaque interface, storage or
 * input where the policy forbids it, or a device that is simply not on
 * the allowlist.
 *
 * This only reads what libusb enumerates; it changes nothing and talks
 * to no device. It is still about physical access - the findings only
 * mean something on a host whose USB ports are supposed to be
 * controlled - so it is for an authorized assessment of a device or
 * kiosk you own or are engaged to test.
 *
 * | Finding | Severity | Raised when |
 * |---|---|---|
 * | @c usb.device-present | info | a device is attached (one per device) |
 * | @c usb.input-and-storage | high | one device is both HID and mass-storage |
 * | @c usb.mass-storage | medium | storage is attached and the policy forbids it |
 * | @c usb.hid-input | low | an input device is attached and the policy forbids it |
 * | @c usb.app-specific-interface | medium | an application-specific (often DFU) interface is exposed |
 * | @c usb.vendor-specific | low | a vendor-specific (opaque) interface is exposed |
 * | @c usb.unexpected-device | medium | the device is not on the allowlist |
 * | @c usb.none | info | nothing is attached (or nothing could be read) |
 *
 * A finding's subject is the device's @c VID:PID, which is stable
 * between runs; the bus/address, which are not, stay in the log detail.
 */

#ifndef __TAPI_USB_AUDIT_H__
#define __TAPI_USB_AUDIT_H__

#include "te_errno.h"
#include "rcf_rpc.h"

#include "tapi_cybersec.h"

#ifdef __cplusplus
extern "C" {
#endif

/** What the attached USB devices are expected to be. */
typedef struct tapi_usb_audit_policy {
    /** An input (HID) device is acceptable. */
    bool allow_hid;
    /** A mass-storage device is acceptable. */
    bool allow_mass_storage;
    /** Flag a single device that is both input and storage (BadUSB). */
    bool flag_input_storage;
    /**
     * Allowlist of acceptable devices as @c "VID:PID" strings (four hex
     * digits each, lower case), @c NULL-terminated, or @c NULL for no
     * allowlist (then @c usb.unexpected-device is never raised).
     */
    const char *const *allow;
} tapi_usb_audit_policy;

/**
 * The default: input and storage both allowed, the input+storage
 * composite flagged, and no allowlist.
 */
extern const tapi_usb_audit_policy tapi_usb_default_audit_policy;

/**
 * Read the agent's USB posture into @p report.
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  policy   What is expected, or @c NULL for the default.
 * @param[out] report   Report to append findings to.
 *
 * @return Status code of reading the posture, not its verdict.
 */
extern te_errno tapi_usb_audit(rcf_rpc_server *rpcs,
                               const tapi_usb_audit_policy *policy,
                               tapi_cybersec_report *report);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TAPI_USB_AUDIT_H__ */

/**@} <!-- END tapi_usb_audit --> */
