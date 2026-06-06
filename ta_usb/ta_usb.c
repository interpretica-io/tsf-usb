/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Agent-side USB enumeration over libusb-1.0
 *
 * Written against the libusb-1.0 synchronous enumeration API
 * (libusb_get_device_list and the descriptor getters). Read-only: it
 * opens a device only to read its string descriptors, and transfers
 * nothing. The context is brought up and down inside each call, so
 * nothing has to survive between calls.
 */

#define TE_LGR_USER     "TA USB"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>

#include <libusb-1.0/libusb.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "logger_api.h"

#include "ta_usb.h"

/** A libusb return code turned into a TE status, with the message logged. */
static te_errno
usb_rc(int r, const char *what)
{
    if (r >= 0)
        return 0;

    ERROR("%s: libusb %d (%s)", what, r, libusb_error_name(r));
    switch (r)
    {
        case LIBUSB_ERROR_INVALID_PARAM:
            return TE_RC(TE_TA_UNIX, TE_EINVAL);
        case LIBUSB_ERROR_ACCESS:
            return TE_RC(TE_TA_UNIX, TE_EACCES);
        case LIBUSB_ERROR_NO_DEVICE:
        case LIBUSB_ERROR_NOT_FOUND:
            return TE_RC(TE_TA_UNIX, TE_ENODEV);
        case LIBUSB_ERROR_NO_MEM:
            return TE_RC(TE_TA_UNIX, TE_ENOMEM);
        case LIBUSB_ERROR_TIMEOUT:
            return TE_RC(TE_TA_UNIX, TE_ETIMEDOUT);
        default:
            return TE_RC(TE_TA_UNIX, TE_EFAIL);
    }
}

/** Append the active configuration's interface classes as "03,08". */
static void
usb_iface_classes(libusb_device *dev, te_string *dest)
{
    struct libusb_config_descriptor *config = NULL;
    bool first = true;
    int i;
    int j;

    if (libusb_get_active_config_descriptor(dev, &config) != 0 &&
        libusb_get_config_descriptor(dev, 0, &config) != 0)
        return;

    for (i = 0; i < config->bNumInterfaces; i++)
    {
        const struct libusb_interface *iface = &config->interface[i];

        for (j = 0; j < iface->num_altsetting; j++)
        {
            te_string_append(dest, "%s%02x", first ? "" : ",",
                             iface->altsetting[j].bInterfaceClass);
            first = false;
        }
    }

    libusb_free_config_descriptor(config);
}

/**
 * Read a device's string descriptor by index into @p dest. Opens the
 * device; a failure to open (privilege, a kernel driver) is not an
 * error - the string is simply left empty.
 */
static void
usb_string(libusb_device *dev, uint8_t index, te_string *dest)
{
    libusb_device_handle *handle = NULL;
    unsigned char buf[256];

    if (index == 0 || libusb_open(dev, &handle) != 0)
        return;

    if (libusb_get_string_descriptor_ascii(handle, index, buf,
                                            sizeof(buf)) > 0)
    {
        te_string_append(dest, "%s", (char *)buf);
    }

    libusb_close(handle);
}

/** One device's summary line for ta_usb_list(). */
static void
usb_device_line(libusb_device *dev, const struct libusb_device_descriptor *d,
                te_string *result)
{
    te_string ifaces = TE_STRING_INIT;
    te_string manufacturer = TE_STRING_INIT;
    te_string product = TE_STRING_INIT;
    te_string serial = TE_STRING_INIT;

    usb_iface_classes(dev, &ifaces);
    usb_string(dev, d->iManufacturer, &manufacturer);
    usb_string(dev, d->iProduct, &product);
    usb_string(dev, d->iSerialNumber, &serial);

    te_string_append(result,
        "%d\t%d\t%d\t%04x\t%04x\t%02x\t%02x\t%02x\t%04x\t%d\t%d\t%s\t%s\t%s\t%s\n",
        libusb_get_bus_number(dev), libusb_get_device_address(dev),
        libusb_get_port_number(dev), d->idVendor, d->idProduct,
        d->bDeviceClass, d->bDeviceSubClass, d->bDeviceProtocol, d->bcdUSB,
        libusb_get_device_speed(dev), d->bNumConfigurations,
        te_string_value(&ifaces),
        te_string_value(&manufacturer), te_string_value(&product),
        te_string_value(&serial));

    te_string_free(&ifaces);
    te_string_free(&manufacturer);
    te_string_free(&product);
    te_string_free(&serial);
}

/* See description in ta_usb.h */
te_errno
ta_usb_list(int *count, te_string *result)
{
    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    ssize_t n;
    ssize_t i;
    te_errno rc;

    *count = 0;

    rc = usb_rc(libusb_init(&ctx), "libusb_init");
    if (rc != 0)
        return rc;

    n = libusb_get_device_list(ctx, &list);
    if (n < 0)
    {
        rc = usb_rc((int)n, "libusb_get_device_list");
        libusb_exit(ctx);
        return rc;
    }

    for (i = 0; i < n; i++)
    {
        struct libusb_device_descriptor d;

        if (libusb_get_device_descriptor(list[i], &d) != 0)
            continue;
        usb_device_line(list[i], &d, result);
        (*count)++;
    }

    libusb_free_device_list(list, 1);
    libusb_exit(ctx);

    return 0;
}

/** Append the config/interface/endpoint lines of one device. */
static void
usb_device_dump(libusb_device *dev, const struct libusb_device_descriptor *d,
                te_string *result)
{
    uint8_t c;

    te_string_append(result,
        "device\t%04x\t%04x\t%02x\t%02x\t%02x\t%04x\t%d\n",
        d->idVendor, d->idProduct, d->bDeviceClass, d->bDeviceSubClass,
        d->bDeviceProtocol, d->bcdUSB, d->bNumConfigurations);

    for (c = 0; c < d->bNumConfigurations; c++)
    {
        struct libusb_config_descriptor *config = NULL;
        int i;
        int j;
        int e;

        if (libusb_get_config_descriptor(dev, c, &config) != 0)
            continue;

        te_string_append(result, "config\t%d\t%d\t%u\n",
                         config->bConfigurationValue, config->bNumInterfaces,
                         (unsigned)config->MaxPower);

        for (i = 0; i < config->bNumInterfaces; i++)
        {
            const struct libusb_interface *iface = &config->interface[i];

            for (j = 0; j < iface->num_altsetting; j++)
            {
                const struct libusb_interface_descriptor *alt =
                    &iface->altsetting[j];

                te_string_append(result,
                    "interface\t%d\t%d\t%02x\t%02x\t%02x\t%d\n",
                    alt->bInterfaceNumber, alt->bAlternateSetting,
                    alt->bInterfaceClass, alt->bInterfaceSubClass,
                    alt->bInterfaceProtocol, alt->bNumEndpoints);

                for (e = 0; e < alt->bNumEndpoints; e++)
                {
                    const struct libusb_endpoint_descriptor *ep =
                        &alt->endpoint[e];

                    te_string_append(result,
                        "endpoint\t%02x\t%02x\t%u\n",
                        ep->bEndpointAddress, ep->bmAttributes,
                        (unsigned)ep->wMaxPacketSize);
                }
            }
        }

        libusb_free_config_descriptor(config);
    }
}

/* See description in ta_usb.h */
te_errno
ta_usb_device(int bus, int addr, te_string *result)
{
    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    ssize_t n;
    ssize_t i;
    bool found = false;
    te_errno rc;

    rc = usb_rc(libusb_init(&ctx), "libusb_init");
    if (rc != 0)
        return rc;

    n = libusb_get_device_list(ctx, &list);
    if (n < 0)
    {
        rc = usb_rc((int)n, "libusb_get_device_list");
        libusb_exit(ctx);
        return rc;
    }

    for (i = 0; i < n && !found; i++)
    {
        struct libusb_device_descriptor d;

        if (libusb_get_bus_number(list[i]) != bus ||
            libusb_get_device_address(list[i]) != addr)
            continue;
        if (libusb_get_device_descriptor(list[i], &d) != 0)
            continue;
        usb_device_dump(list[i], &d, result);
        found = true;
    }

    libusb_free_device_list(list, 1);
    libusb_exit(ctx);

    if (!found)
    {
        ERROR("No USB device at bus %d address %d", bus, addr);
        return TE_RC(TE_TA_UNIX, TE_ENODEV);
    }

    return 0;
}
