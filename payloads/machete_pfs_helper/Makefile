# MacheteSVG - PFS Helper Daemon Makefile
# Copyright (C) 2026 MacheteSVG Team
# SPDX-License-Identifier: GPL-3.0-or-later

SDK_DIR ?= $(CURDIR)/../../.deps/native/ps5-payload-sdk
PS5_PAYLOAD_SDK ?= $(SDK_DIR)

CC := $(PS5_PAYLOAD_SDK)/bin/prospero-clang
CFLAGS := -Wall -O2 -g -lSceFsInternalForVsh -lSceNet

TARGET := machete_pfs_helper.elf

all: $(TARGET)

$(TARGET): main.c
	PATH="/opt/homebrew/opt/llvm/bin:$(PATH)" $(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
