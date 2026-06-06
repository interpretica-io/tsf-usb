/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief USB TAPI: RPC client wrappers
 *
 * The rcf_rpc_call() boilerplate behind tapi_usb. The RPCs return
 * te_errno; an RPC transport failure is mapped to TE_ECORRUPTED.
 */

#define TE_LGR_USER     "TAPI USB RPC"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "logger_api.h"
#include "tapi_rpc_internal.h"
#include "tarpc.h"

#include "tapi_usb_rpc.h"

#define CHECK_RPC_ERRNO_UNCHANGED(_func, _var) \
    CHECK_RETVAL_VAR_ERR_COND(_func, _var, false,                    \
                              TE_RC(TE_TAPI, TE_ECORRUPTED), false)

/* Append an RPC string result, when there is one. */
static void
take_string(te_string *dst, const char *src)
{
    if (dst != NULL && src != NULL)
        te_string_append(dst, "%s", src);
}

/* See description in tapi_usb_rpc.h */
te_errno
rpc_usb_list(rcf_rpc_server *rpcs, int *count, te_string *result)
{
    tarpc_usb_list_in in;
    tarpc_usb_list_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    rcf_rpc_call(rpcs, "usb_list", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(usb_list, out.retval);
    TAPI_RPC_LOG(rpcs, usb_list, "", "%r count=%d", out.retval, out.count);

    if (out.retval == 0)
    {
        if (count != NULL)
            *count = out.count;
        take_string(result, out.result);
    }
    RETVAL_TE_ERRNO(usb_list, out.retval);
}

/* See description in tapi_usb_rpc.h */
te_errno
rpc_usb_device(rcf_rpc_server *rpcs, int bus, int addr, te_string *result)
{
    tarpc_usb_device_in in;
    tarpc_usb_device_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.bus = bus;
    in.addr = addr;

    rcf_rpc_call(rpcs, "usb_device", &in, &out);
    CHECK_RPC_ERRNO_UNCHANGED(usb_device, out.retval);
    TAPI_RPC_LOG(rpcs, usb_device, "bus=%d addr=%d", "%r", bus, addr,
                 out.retval);

    if (out.retval == 0)
        take_string(result, out.result);
    RETVAL_TE_ERRNO(usb_device, out.retval);
}
