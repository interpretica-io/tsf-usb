/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief RPC for USB enumeration
 *
 * The RPCs of rpcs_usb, a thin layer over ta_usb, which enumerates the
 * agent's USB devices in the RPC server process over libusb-1.0. Add
 * this file to the rpcxdr definitions of the engine platform and of the
 * agent platform:
 *
 *   TE_LIB_PARMS([rpcxdr], [<platform>], [],
 *                [--with-rpcdefs=tarpc_job.x.m4,../ta_usb/usb_rpc.x.m4])
 *
 * No handle survives between calls: enumeration is a whole snapshot in
 * one call, and a device is named by bus/address for a detailed read.
 * Results that are lists come back as newline-separated text, one
 * record per line with tab-separated fields - the engine side parses
 * them, the same shape tsf-upnp uses.
 */

/* usb_list(): one summary line per device (see ta_usb.h for the fields). */
struct tarpc_usb_list_in {
    struct tarpc_in_arg common;
};

struct tarpc_usb_list_out {
    struct tarpc_out_arg common;

    tarpc_int       retval;
    tarpc_int       count;
    string          result<>;
};

/* usb_device(): the full descriptor dump of one device, by bus/address. */
struct tarpc_usb_device_in {
    struct tarpc_in_arg common;

    tarpc_int       bus;
    tarpc_int       addr;
};

struct tarpc_usb_device_out {
    struct tarpc_out_arg common;

    tarpc_int       retval;
    string          result<>;
};

program usb
{
    version ver0
    {
        RPC_DEF(usb_list)
        RPC_DEF(usb_device)
    } = 1;
} = 26;
