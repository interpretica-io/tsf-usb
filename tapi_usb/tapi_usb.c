/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Enumerating USB devices on an agent from a test
 *
 * The engine-side layer over the usb_* RPCs: it asks the agent to
 * enumerate its USB devices and parses the newline/tab record text into
 * a vector of #tapi_usb_device.
 */

#define TE_LGR_USER     "TAPI USB"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "te_vector.h"
#include "logger_api.h"

#include "tapi_usb.h"
#include "tapi_usb_rpc.h"

/** The number of tab-separated fields in a usb_list() line. */
#define TAPI_USB_FIELDS 15

/**
 * Copy the @p n fields of @p line (up to a newline) into @p out, each a
 * heap string. Returns the count found; missing fields are @c NULL.
 */
static size_t
usb_split(const char *line, size_t line_len, char **out, size_t n)
{
    const char *p = line;
    const char *end = line + line_len;
    size_t i = 0;

    for (i = 0; i < n; i++)
        out[i] = NULL;

    for (i = 0; i < n && p <= end; i++)
    {
        const char *tab = memchr(p, '\t', (size_t)(end - p));
        size_t len = (tab != NULL && tab < end) ? (size_t)(tab - p) :
                     (size_t)(end - p);

        out[i] = TE_STRNDUP(p, len);
        if (tab == NULL || tab >= end)
        {
            i++;
            break;
        }
        p = tab + 1;
    }

    return i;
}

/** A non-empty field as a heap string, or @c NULL when it was empty. */
static char *
usb_str_or_null(char *field)
{
    if (field == NULL || field[0] == '\0')
        return NULL;
    return TE_STRDUP(field);
}

/** Parse "03,08" into @p device's iface_classes array. */
static void
usb_parse_iface_classes(const char *csv, tapi_usb_device *device)
{
    const char *p = csv;

    device->iface_classes = NULL;
    device->n_iface_classes = 0;
    if (csv == NULL || csv[0] == '\0')
        return;

    while (p != NULL && *p != '\0')
    {
        unsigned long value = strtoul(p, NULL, 16);
        uint8_t cls = (uint8_t)value;
        const char *comma = strchr(p, ',');

        device->iface_classes = TE_REALLOC(device->iface_classes,
                                   (device->n_iface_classes + 1) *
                                   sizeof(*device->iface_classes));
        device->iface_classes[device->n_iface_classes++] = cls;

        p = comma != NULL ? comma + 1 : NULL;
    }
}

/** Parse one usb_list() line into a device; returns false on a short line. */
static bool
usb_parse_line(const char *line, size_t len, tapi_usb_device *device)
{
    char *f[TAPI_USB_FIELDS];
    size_t n = usb_split(line, len, f, TAPI_USB_FIELDS);
    bool ok = false;
    size_t i;

    if (n < TAPI_USB_FIELDS)
        goto out;

    memset(device, 0, sizeof(*device));
    device->bus = (int)strtol(f[0], NULL, 10);
    device->addr = (int)strtol(f[1], NULL, 10);
    device->port = (int)strtol(f[2], NULL, 10);
    device->vid = (uint16_t)strtoul(f[3], NULL, 16);
    device->pid = (uint16_t)strtoul(f[4], NULL, 16);
    device->dev_class = (uint8_t)strtoul(f[5], NULL, 16);
    device->dev_subclass = (uint8_t)strtoul(f[6], NULL, 16);
    device->dev_protocol = (uint8_t)strtoul(f[7], NULL, 16);
    device->bcd_usb = (uint16_t)strtoul(f[8], NULL, 16);
    device->speed = (int)strtol(f[9], NULL, 10);
    device->n_configs = (int)strtol(f[10], NULL, 10);
    usb_parse_iface_classes(f[11], device);
    device->manufacturer = usb_str_or_null(f[12]);
    device->product = usb_str_or_null(f[13]);
    device->serial = usb_str_or_null(f[14]);
    ok = true;

out:
    for (i = 0; i < n; i++)
        free(f[i]);
    return ok;
}

/* See description in tapi_usb.h */
te_errno
tapi_usb_list(rcf_rpc_server *rpcs, te_vec *devices)
{
    te_string raw = TE_STRING_INIT;
    const char *line;
    te_errno rc;

    *devices = (te_vec)TE_VEC_INIT(tapi_usb_device);

    rc = rpc_usb_list(rpcs, NULL, &raw);
    if (rc != 0)
    {
        te_string_free(&raw);
        return rc;
    }

    line = te_string_value(&raw);
    while (line != NULL && *line != '\0')
    {
        const char *nl = strchr(line, '\n');
        size_t len = nl != NULL ? (size_t)(nl - line) : strlen(line);
        tapi_usb_device device;

        if (len != 0 && usb_parse_line(line, len, &device))
            TE_VEC_APPEND(devices, device);

        line = nl != NULL ? nl + 1 : NULL;
    }

    te_string_free(&raw);

    return 0;
}

/* See description in tapi_usb.h */
te_errno
tapi_usb_device_dump(rcf_rpc_server *rpcs, int bus, int addr, te_string *dump)
{
    return rpc_usb_device(rpcs, bus, addr, dump);
}

/* See description in tapi_usb.h */
bool
tapi_usb_has_class(const tapi_usb_device *device, uint8_t usb_class)
{
    size_t i;

    for (i = 0; i < device->n_iface_classes; i++)
    {
        if (device->iface_classes[i] == usb_class)
            return true;
    }
    /* A simple device may carry its class only at the device level. */
    return device->dev_class == usb_class;
}

/* See description in tapi_usb.h */
const tapi_usb_device *
tapi_usb_find(const te_vec *devices, uint16_t vid, uint16_t pid)
{
    const tapi_usb_device *device;

    TE_VEC_FOREACH((te_vec *)devices, device)
    {
        if (device->vid == vid && device->pid == pid)
            return device;
    }

    return NULL;
}

/* See description in tapi_usb.h */
const char *
tapi_usb_class2str(uint8_t usb_class)
{
    switch (usb_class)
    {
        case 0x00: return "per-interface";
        case 0x01: return "audio";
        case 0x02: return "communications";
        case 0x03: return "HID";
        case 0x05: return "physical";
        case 0x06: return "image";
        case 0x07: return "printer";
        case 0x08: return "mass-storage";
        case 0x09: return "hub";
        case 0x0a: return "CDC-data";
        case 0x0b: return "smart-card";
        case 0x0d: return "content-security";
        case 0x0e: return "video";
        case 0x0f: return "personal-healthcare";
        case 0x10: return "audio-video";
        case 0xdc: return "diagnostic";
        case 0xe0: return "wireless";
        case 0xef: return "miscellaneous";
        case 0xfe: return "application-specific";
        case 0xff: return "vendor-specific";
        default:   return "unknown";
    }
}

/* See description in tapi_usb.h */
void
tapi_usb_device_log(const tapi_usb_device *device)
{
    te_string classes = TE_STRING_INIT;
    size_t i;

    for (i = 0; i < device->n_iface_classes; i++)
    {
        te_string_append(&classes, "%s%s", i != 0 ? ", " : "",
                         tapi_usb_class2str(device->iface_classes[i]));
    }

    RING("USB %04x:%04x on bus %d addr %d: device-class %s, interfaces [%s], "
         "USB %x.%02x\n  %s %s %s",
         device->vid, device->pid, device->bus, device->addr,
         tapi_usb_class2str(device->dev_class), te_string_value(&classes),
         device->bcd_usb >> 8, device->bcd_usb & 0xff,
         device->manufacturer != NULL ? device->manufacturer : "?",
         device->product != NULL ? device->product : "?",
         device->serial != NULL ? device->serial : "");

    te_string_free(&classes);
}

/* See description in tapi_usb.h */
void
tapi_usb_list_free(te_vec *devices)
{
    tapi_usb_device *device;

    TE_VEC_FOREACH(devices, device)
    {
        free(device->iface_classes);
        free(device->manufacturer);
        free(device->product);
        free(device->serial);
    }
    te_vec_free(devices);
}
