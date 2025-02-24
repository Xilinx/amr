# Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT

# set workspace to current directory
setws .

# read in passed arguments from command line
set xsa [lindex $argv 0]
puts "config xsa ==> $xsa"

set os [lindex $argv 1]
puts "config os ==> $os"

platform create -name "amc_bsp" -hw $xsa -proc "cortexr5_0" -os $os 

#bsp config stdin cips_pspmc_0_psv_sbsauart_0
#bsp config stdout cips_pspmc_0_psv_sbsauart_0

bsp setlib -name xilfpga
bsp setlib -name xilmailbox
bsp setlib -name xilloader
bsp getlibs
hsi set_property CONFIG.xloader_mode "client" [hsi::get_libs xilloader]

platform generate 


