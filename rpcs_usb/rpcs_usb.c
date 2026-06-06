/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief USB RPC server library
 *
 * The usb_* RPCs (see usb_rpc.x.m4) on top of ta_usb.
 * TARPC_FUNC_STATIC() binds an RPC to the function of the same name,
 * so each RPC has a plain C function first and the wrapper after it.
 */

#define TE_LGR_USER     "RPC USB"

#include "te_config.h"

#include <string.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"
#include "rpc_server.h"

#include "ta_usb.h"

/* Hand a te_string result over to an RPC string field (never NULL). */
static char *
take(te_string *str)
{
    return str->ptr != NULL ? str->ptr : TE_STRDUP("");
}

static te_errno
usb_list(int *count, char **result)
{
    te_string r = TE_STRING_INIT;
    te_errno rc = ta_usb_list(count, &r);

    *result = take(&r);
    return rc;
}

TARPC_FUNC_STATIC(usb_list, {},
{
    int count = 0;

    MAKE_CALL(out->retval = func(&count, &out->result));
    out->count = count;
    out->common.errno_changed = false;
})

static te_errno
usb_device(int bus, int addr, char **result)
{
    te_string r = TE_STRING_INIT;
    te_errno rc = ta_usb_device(bus, addr, &r);

    *result = take(&r);
    return rc;
}

TARPC_FUNC_STATIC(usb_device, {},
{
    MAKE_CALL(out->retval = func(in->bus, in->addr, &out->result));
    out->common.errno_changed = false;
})
