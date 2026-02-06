# Copyright 2024 DPDK Demo
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Application name
APP = dpdk_filter

# Source files
SRCS = main.c

# Compiler
CC = gcc

# CFLAGS
CFLAGS = -O3 -g -Wall -Wextra
CFLAGS += -std=c11

# Try to find DPDK using pkg-config (DPDK 20.11+)
PKG_CONFIG ?= pkg-config
DPDK_CFLAGS := $(shell $(PKG_CONFIG) --exists libdpdk 2>/dev/null && $(PKG_CONFIG) --cflags libdpdk || echo "")
DPDK_LIBS := $(shell $(PKG_CONFIG) --exists libdpdk 2>/dev/null && $(PKG_CONFIG) --libs libdpdk || echo "")

# If pkg-config works, use it (preferred method)
ifneq ($(DPDK_CFLAGS),)
    CFLAGS += $(DPDK_CFLAGS)
    LDLIBS = $(DPDK_LIBS) -lpthread -ldl -lm
    DPDK_FOUND = 1
endif

# If pkg-config doesn't work, try manual paths
ifeq ($(DPDK_FOUND),)
    # Check for installed DPDK
    ifneq ($(wildcard /usr/local/include/dpdk),)
        DPDK_INC = /usr/local/include
        DPDK_LIB = /usr/local/lib/x86_64-linux-gnu
    else ifneq ($(wildcard /usr/include/dpdk),)
        DPDK_INC = /usr/include
        DPDK_LIB = /usr/lib/x86_64-linux-gnu
    else ifdef RTE_SDK
        # Use RTE_SDK if set
        ifneq ($(wildcard $(RTE_SDK)/include/dpdk),)
            DPDK_INC = $(RTE_SDK)/include
            DPDK_LIB = $(RTE_SDK)/lib/x86_64-linux-gnu
        else ifneq ($(wildcard $(RTE_SDK)/build/include/dpdk),)
            DPDK_INC = $(RTE_SDK)/build/include
            DPDK_LIB = $(RTE_SDK)/build/lib
        else ifneq ($(wildcard $(RTE_SDK)/include/rte_common.h),)
            DPDK_INC = $(RTE_SDK)/include
            DPDK_LIB = $(RTE_SDK)/lib
        else ifneq ($(wildcard $(RTE_SDK)/lib/librte_eal.so),)
            DPDK_INC = $(RTE_SDK)/include
            DPDK_LIB = $(RTE_SDK)/lib
        else
            $(error DPDK not found at RTE_SDK=$(RTE_SDK). Please check RTE_SDK path)
        endif
    else
        # Try local build - check multiple possible locations
        ifneq ($(wildcard dpdk-21.11/build/include/rte_common.h),)
            DPDK_INC = dpdk-21.11/build/include
            DPDK_LIB = dpdk-21.11/build/lib
        else ifneq ($(wildcard dpdk-21.11/build/lib/librte_eal.so),)
            DPDK_INC = dpdk-21.11/build/include
            DPDK_LIB = dpdk-21.11/build/lib
        else ifneq ($(wildcard dpdk-21.11/build/include/dpdk),)
            DPDK_INC = dpdk-21.11/build/include
            DPDK_LIB = dpdk-21.11/build/lib
        else
            $(error DPDK not found. Please run: ./install_dpdk.sh)
        endif
    endif
    
    # DPDK 21.11+ header files are in the source directory
    # rte_config.h is in build directory (created as symlink to rte_build_config.h)
    # Add source directory include paths and build directory for config
    # Also add build/lib/eal/include for platform-specific headers
    CFLAGS += -I$(DPDK_INC) -Idpdk-21.11/build -Idpdk-21.11/build/lib/eal/include \
              -Idpdk-21.11/build/lib/eal/linux/include -Idpdk-21.11/build/lib/eal/x86/include \
              -Idpdk-21.11/lib/eal/include -Idpdk-21.11/lib/eal/common/include \
              -Idpdk-21.11/lib/eal/linux/include -Idpdk-21.11/lib/eal/x86/include \
              -Idpdk-21.11/lib/kvargs -Idpdk-21.11/lib/ethdev -Idpdk-21.11/lib/net \
              -Idpdk-21.11/lib/mbuf -Idpdk-21.11/lib/mempool -Idpdk-21.11/lib/ring \
              -Idpdk-21.11/lib/kni -Idpdk-21.11/lib/pci -Idpdk-21.11/lib/bus/pci \
              -Idpdk-21.11/lib/bus/vdev -Idpdk-21.11/lib/hash -Idpdk-21.11/lib/timer
    DPDK_LIBS = -L$(DPDK_LIB) -lrte_kni -lrte_eal -lrte_mempool -lrte_ring \
                -lrte_mbuf -lrte_ethdev -lrte_net -lrte_pci -lrte_bus_pci \
                -lrte_bus_vdev -lrte_kvargs -lrte_hash -lrte_timer
endif

# Additional libraries (only if not set by pkg-config)
ifeq ($(DPDK_FOUND),)
    LDLIBS = $(DPDK_LIBS) -lpthread -ldl -lm
endif

# Build rules
.PHONY: all clean install check-dpdk

all: check-dpdk $(APP)

check-dpdk:
	@echo "Checking DPDK..."
	@if [ -z "$(DPDK_CFLAGS)" ] && [ -z "$(DPDK_INC)" ]; then \
		echo "Error: DPDK not found!"; \
		echo ""; \
		echo "Please install DPDK:"; \
		echo "  1. Install dependencies: sudo apt install meson ninja pkg-config python3-pip"; \
		echo "  2. Run: ./install_dpdk.sh"; \
		echo ""; \
		echo "Or set RTE_SDK environment variable to point to DPDK installation"; \
		exit 1; \
	fi
	@echo "DPDK found: $(if $(DPDK_CFLAGS),pkg-config,$(DPDK_INC))"

$(APP): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

install: $(APP)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(APP) $(DESTDIR)/usr/local/bin/

clean:
	rm -f $(APP) *.o
