# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.

TARGET_MODULE:=ami

obj-m += $(TARGET_MODULE).o

$(TARGET_MODULE)-objs += ami_top.o
$(TARGET_MODULE)-objs += ami_pcie.o
$(TARGET_MODULE)-objs += ami_pci_dbg.o
$(TARGET_MODULE)-objs += ami_vsec.o
$(TARGET_MODULE)-objs += ami_amc_control.o
$(TARGET_MODULE)-objs += ami_sensor.o
$(TARGET_MODULE)-objs += ami_cdev.o
$(TARGET_MODULE)-objs += ami_hwmon.o
$(TARGET_MODULE)-objs += ami_sysfs.o
$(TARGET_MODULE)-objs += ami_program.o
$(TARGET_MODULE)-objs += amc_proxy.o
$(TARGET_MODULE)-objs += ami_gcq.o

CFLAGS       :=
DRIVER_DIR   := $(PWD)
INCLUDE_DIRS := $(DRIVER_DIR)
KERNEL_DIR   := /lib/modules/`uname -r`/build

#CFLAGS_file_name will apply the macros to those files
# CFLAGS_versal_amc.o:=-DDEBUG

#To apply the macro to all the source files compiled with this makefile
# ccflags-y:=-DDEBUG
ccflags-y :=-DDEBUG -DVERBOSE_DEBUG
ccflags-y += $(addprefix -I,$(INCLUDE_DIRS)) $(CFLAGS)

all: clean
	$(MAKE) -C ${KERNEL_DIR} M=${PWD} modules

clean:
	$(MAKE) -C ${KERNEL_DIR} M=${PWD} clean
