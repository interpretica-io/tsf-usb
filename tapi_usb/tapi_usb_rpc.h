/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief USB TAPI: RPC client wrappers
 *
 * Client wrappers of the usb_* RPCs, see usb_rpc.x.m4. Tests use
 * tapi_usb.h; these are the calls behind it, one per RPC.
 */

#ifndef __TAPI_USB_RPC_H__
#define __TAPI_USB_RPC_H__

#include "te_errno.h"
#include "te_string.h"
#include "rcf_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * List the agent's USB devices (raw record text).
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[out] count    Number of devices, or @c NULL.
 * @param[out] result   The device lines, or @c NULL.
 *
 * @return Status code.
 */
extern te_errno rpc_usb_list(rcf_rpc_server *rpcs, int *count,
                             te_string *result);

/**
 * Read one device's descriptor dump (raw record text).
 *
 * @param[in]  rpcs     RPC server on the agent.
 * @param[in]  bus      Bus number.
 * @param[in]  addr     Device address.
 * @param[out] result   The descriptor lines, or @c NULL.
 *
 * @return Status code.
 */
extern te_errno rpc_usb_device(rcf_rpc_server *rpcs, int bus, int addr,
                               te_string *result);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TAPI_USB_RPC_H__ */
