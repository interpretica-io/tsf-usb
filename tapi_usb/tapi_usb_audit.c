/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief What the attached USB devices are worth as a security posture
 *
 * Enumerates with tapi_usb_list() and classifies each device into
 * tsf-cybersec findings. Read-only; nothing is sent to a device.
 */

#define TE_LGR_USER     "TAPI USB AUDIT"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "logger_api.h"

#include "tapi_usb.h"
#include "tapi_usb_audit.h"

/** USB class codes the audit cares about. */
#define USB_CLASS_HID           0x03
#define USB_CLASS_MASS_STORAGE  0x08
#define USB_CLASS_APP_SPECIFIC  0xfe  /**< DFU lives here (subclass 0x01). */
#define USB_CLASS_VENDOR        0xff

/* See description in tapi_usb_audit.h */
const tapi_usb_audit_policy tapi_usb_default_audit_policy = {
    .allow_hid = true,
    .allow_mass_storage = true,
    .flag_input_storage = true,
    .allow = NULL,
};

/** Is "VID:PID" (as formatted below) on the allowlist? */
static bool
usb_allowed(const tapi_usb_audit_policy *policy, const char *vidpid)
{
    size_t i;

    if (policy->allow == NULL)
        return true;    /* no allowlist means "do not judge membership" */
    for (i = 0; policy->allow[i] != NULL; i++)
    {
        if (strcasecmp(policy->allow[i], vidpid) == 0)
            return true;
    }
    return false;
}

/** Classify one device into findings. */
static void
usb_audit_device(const tapi_usb_device *device,
                 const tapi_usb_audit_policy *policy,
                 tapi_cybersec_report *report)
{
    char subject[16];
    bool hid = tapi_usb_has_class(device, USB_CLASS_HID);
    bool storage = tapi_usb_has_class(device, USB_CLASS_MASS_STORAGE);
    const char *product = device->product != NULL ? device->product : "?";

    TE_SPRINTF(subject, "%04x:%04x", device->vid, device->pid);

    tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_INFO,
        "usb.device-present", subject,
        "%s '%s' on bus %d addr %d, device-class %s", subject, product,
        device->bus, device->addr, tapi_usb_class2str(device->dev_class));

    /*
     * The BadUSB shape: one device that is both a keyboard and a disk.
     * Either half may be benign alone; together on one device they are
     * the thing a malicious stick impersonates.
     */
    if (policy->flag_input_storage && hid && storage)
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_HIGH,
            "usb.input-and-storage", subject,
            "one device exposes both HID (input) and mass-storage - the "
            "BadUSB composite shape ('%s')", product);
    }

    if (storage && !policy->allow_mass_storage)
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_MEDIUM,
            "usb.mass-storage", subject,
            "mass-storage device attached where the policy forbids it "
            "('%s')", product);
    }

    if (hid && !policy->allow_hid)
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_LOW,
            "usb.hid-input", subject,
            "input device attached where the policy forbids it ('%s')",
            product);
    }

    if (tapi_usb_has_class(device, USB_CLASS_APP_SPECIFIC))
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_MEDIUM,
            "usb.app-specific-interface", subject,
            "an application-specific interface is exposed (class 0xfe, "
            "often firmware update / DFU) on '%s'", product);
    }

    if (tapi_usb_has_class(device, USB_CLASS_VENDOR))
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_LOW,
            "usb.vendor-specific", subject,
            "a vendor-specific (opaque) interface is exposed on '%s'",
            product);
    }

    if (!usb_allowed(policy, subject))
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_MEDIUM,
            "usb.unexpected-device", subject,
            "device is not on the allowlist ('%s')", product);
    }
}

/* See description in tapi_usb_audit.h */
te_errno
tapi_usb_audit(rcf_rpc_server *rpcs, const tapi_usb_audit_policy *policy,
               tapi_cybersec_report *report)
{
    te_vec devices = TE_VEC_INIT(tapi_usb_device);
    const tapi_usb_device *device;
    te_errno rc;

    if (policy == NULL)
        policy = &tapi_usb_default_audit_policy;

    rc = tapi_usb_list(rpcs, &devices);
    if (rc != 0)
        return rc;

    if (te_vec_size(&devices) == 0)
    {
        tapi_cybersec_report_add(report, TAPI_CYBERSEC_SEV_INFO,
            "usb.none", "-", "no USB devices are attached (or none could "
            "be enumerated)");
    }

    TE_VEC_FOREACH(&devices, device)
        usb_audit_device(device, policy, report);

    tapi_usb_list_free(&devices);

    return 0;
}
