#
# Copyright (c) 2024 triaxis s.r.o.
# Licensed under the MIT license. See LICENSE.txt file in the repository root
# for full license information.
#
# nvram/tests/sanity_dw/Include.mk
#
# This is a variant of the basic sanity suite with double-word writes
#

DEFINES += NVRAM_FLASH_DOUBLE_WRITE

override TEST := $(call parentdir, $(TEST))sanity/
