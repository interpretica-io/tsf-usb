# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# External TE library (see TE_EXT_REPO): agent-side USB enumeration over
# libusb-1.0 (read-only). The agent and its RPC server both link it.

headers += files(
    'ta_usb.h',
)

sources += files(
    'ta_usb.c',
)

te_libs += [
    'logger_core',
    'tools',
]

deps += [
    dependency('libusb-1.0'),
]
