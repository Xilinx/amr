# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.


TARGET    := libami.so
BUILD_DIR := ./build

# Driver profile selection: kernel (default) or userspace
# Build with: make DRIVER_PROFILE=userspace
DRIVER_PROFILE ?= kernel

# Select source and include directories based on driver profile
ifeq ($(DRIVER_PROFILE),userspace)
    # VFIO userspace driver
    SRC_DIRS  := ./vfio-driver/src ./src
    INC_DIRS  := ./vfio-driver/include ./include ./src
    CFLAGS_DRIVER := -DAMI_USERSPACE_DRIVER_ONLY
    $(info Building VFIO userspace driver)
else ifeq ($(DRIVER_PROFILE),kernel)
    # Kernel driver (default)
    SRC_DIRS  := ./src
    INC_DIRS  := ./include ./src
    CFLAGS_DRIVER :=
    $(info Building kernel driver)
else
    $(error Invalid DRIVER_PROFILE=$(DRIVER_PROFILE). Must be: kernel or userspace)
endif

# Find all C source files
SRCS := $(shell find $(SRC_DIRS) -name '*.c')

# For userspace driver, exclude ALL kernel-only driver files from ./src
# The VFIO driver provides its own implementations in ./vfio-driver/src
# Only keep common utility files (ami.c, md5.c) from ./src
ifeq ($(DRIVER_PROFILE),userspace)
    SRCS := $(filter-out ./src/ami_device.c ./src/ami_sensor.c ./src/ami_eeprom_access.c ./src/ami_mem_access.c ./src/ami_mfg_info.c ./src/ami_module_access.c ./src/ami_program.c,$(SRCS))
endif

# Prepends BUILD_DIR and appends .o to every src file
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

# Replace .o with .d for dependencies
DEPS := $(OBJS:.o=.d)

ifneq ($(NO_SYSTEM_API),1)
INC_DIRS  += /usr/include/libami/api
endif
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# The -MMD and -MP flags together generate Makefiles for us
CFLAGS  := $(INC_FLAGS) -MMD -MP -Wall -Werror -fpic $(CFLAGS_DRIVER)

# LDFLAGS: -lpthread is only needed for VFIO userspace driver (GCQ logging thread)
ifeq ($(DRIVER_PROFILE),userspace)
    LDFLAGS := -shared -lpthread
else
    LDFLAGS := -shared
endif

#
# Default make
#
all:
	@$(MAKE) -s banner
	@$(MAKE) -s $(BUILD_DIR)/$(TARGET)

#
# Prints new line
#
newline:
	@echo "#######################################################################\n"

#
# Displays important info at the start of the make
#
banner:
	@$(MAKE) -s newline
	@echo "  - $(shell date)"
	@echo "  - $(USER)"
	@echo "  - $(PWD)\n"
	@echo "  - DRIVER_PROFILE : $(DRIVER_PROFILE)"
	@echo "  - CC             : $(CC)"
	@echo "  - CFLAGS         : $(CFLAGS)"
	@echo "  - LDFLAGS        : $(LDFLAGS)"
	@echo "  - INC_DIRS       : $(INC_DIRS)"
	@echo "  - BUILD_DIR      : $(BUILD_DIR)\n"
	@$(MAKE) -s newline

# The final build step
$(BUILD_DIR)/$(TARGET): $(OBJS)
	@echo "Building $@"
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Done\n"
	@$(MAKE) -s newline

# Build step for C source
$(BUILD_DIR)/%.c.o: %.c
	@echo "Building $@"
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# Include the .d makefiles
-include $(DEPS)
