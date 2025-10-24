#!/usr/bin/env python3

# Copyright (C) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# E.g.:
#./scripts/gen_fpt.py <-p rave|v80> [-o output_folder]
#
# Generate FPT binary file with profile input. Default rave profile
#

import os
import yaml
import argparse
from collections import namedtuple


# FPT configuration data
#  ------------------------------
# |   FPT HEADER   (size = 128K) |
#  ------------------------------
# RAVE
#        ------------------------------
#       |  FPT entriy 0  (size = 128K) |
#        ------------------------------
#       |  FPT entriy 1  (size = 128K) |
#        ------------------------------
#       |  FPT entriy 2  (size = 128K) |
#        ------------------------------
# V80
#        ------------------------------
#       |  FPT entriy 0  (size = 128K) |
#        ------------------------------
#       |  FPT entriy 1  (size = 128K) |
#        ------------------------------
#       |  FPT entriy 2  (size = 128K) |
#        ------------------------------

fpt_config_data = """
fpt_header:
  magic_word:       0x92F7A516
  fpt_version:      2
  fpt_header_size:  128
  fpt_entry_size:   128
  num_entries:      3
  fpt_entry_offset: 128 * 1024           # 0x0002_0000

fpt_entry_rave:
    - type:            0x00000E00        # "PDI"
      base_addr:       0x00080000
      partition_size:  58 * 1024 * 1024  # 0x03A0_0000
      partition_flags: 0x00

    - type:            0x00000E00        # "PDI"
      base_addr:       0x03B80000
      partition_size:  58 * 1024 * 1024  # 0x03A0_0000
      partition_flags: 0x00

    - type:            0x00000F00        # "USER"
      base_addr:       0x07680000
      partition_size:  8 * 1024 * 1024  # 0x0080_0000
      partition_flags: 0x00

fpt_entry_v80:
    - type:            0x00000E00        # "PDI"
      base_addr:       0x00080000
      partition_size:  116 * 1024 * 1024 # 0x0740_0000
      partition_flags: 0x00

    - type:            0x00000E00        # "PDI"
      base_addr:       0x07480000
      partition_size:  116 * 1024 * 1024 # 0x0740_0000
      partition_flags: 0x00

    - type:            0x00000F00        # "USER"
      base_addr:       0x0E880000
      partition_size:  16 * 1024 * 1024  # 0x0170_0000
      partition_flags: 0x00
"""

# Constants
U32_SIZE_BYTES = 4
U8_SIZE_BYTES  = 1


# Debug class uses to dump hex output
class hexdump:
    def __init__(self, buf, off=0):
        self.buf = buf
        self.off = off

    def __iter__(self):
        last_bs, last_line = None, None
        for i in range(0, len(self.buf), 16):
            bs = bytearray(self.buf[i : i + 16])
            line = "{:08x}  {:23}  {:23}  |{:16}|".format(
                self.off + i,
                " ".join(("{:02x}".format(x) for x in bs[:8])),
                " ".join(("{:02x}".format(x) for x in bs[8:])),
                "".join((chr(x) if 32 <= x < 127 else "." for x in bs)),
            )
            if bs == last_bs:
                line = "*"
            if bs != last_bs or line != last_line:
                yield line
            last_bs, last_line = bs, line
        yield "{:08x}".format(self.off + len(self.buf))

    def __str__(self):
        return "\n".join(self)

    def __repr__(self):
        return "\n".join(self)


# The main loop
def main():

    parser = argparse.ArgumentParser(description='Generate FPT binary file',
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-p', '--profile',     help='profile [rave|v80]', default = 'rave')
    parser.add_argument('-o', '--output_path', help='FPT output path', default = './')
    parser.add_argument('-v', '--verbose',     action='store_true', help="increase output verbosity")
    args = parser.parse_args()

    args.profile = args.profile.lower()
    if args.profile != 'rave' and args.profile != 'v80':
        print('Error: Invalid profile, supported profiles: rave | v80')
        parser.print_help()
        raise SystemExit(1)

    # Step1: Print verbose args
    if args.verbose:
        print('profile:     ' + args.profile)
        print('Output path: ' + args.output_path)

    # Step2: load the FPT configuration data
    data = yaml.safe_load(fpt_config_data)

    # Step3: Parse the FPT header
    magic_word       = data['fpt_header']['magic_word']
    fpt_version      = data['fpt_header']['fpt_version']
    fpt_header_size  = data['fpt_header']['fpt_header_size']
    fpt_entry_size   = data['fpt_header']['fpt_entry_size']
    num_entries      = data['fpt_header']['num_entries']
    fpt_entry_offset = eval(data['fpt_header']['fpt_entry_offset'])

    # Step4: Parse the FPT entries
    fpt_entry = namedtuple('fpt_entry', 'type base_addr partition_size partition_flags')
    fpt_entry_list = []
    for entry in data['fpt_entry_' + args.profile]:
        fpt_entry_list.append(fpt_entry(
                              entry.get('type'),
                              entry.get('base_addr'),
                              eval(entry.get('partition_size')),
                              entry.get('partition_flags')))
    if args.verbose:
        print('\nFPT Header:')
        print('    magic_word:      ', hex(magic_word))
        print('    fpt_version:     ', fpt_version)
        print('    fpt_header_size: ', fpt_header_size)
        print('    fpt_entry_size:  ', fpt_entry_size)
        print('    num_entries:     ', num_entries)
        print('    fpt_entry_offset:', hex(fpt_entry_offset))
        for i, fpt_tuple in enumerate(fpt_entry_list):
            print('FPT entry:',        i)
            print('    type:           ', hex(fpt_tuple.type))
            print('    base_addr:      ', hex(fpt_tuple.base_addr))
            print(f'    partition_size:  {fpt_tuple.partition_size / (1024 * 1024):.2f} MB')
            print('    partition_flags:', hex(fpt_tuple.partition_flags))
        print('')

    # Step5: Create an empty byte array of fixed size & populate
    try:
        fpt_size = fpt_entry_offset * (num_entries + 1)
        fpt_data = bytearray(fpt_size)

        pos = 0
        fpt_data[pos:0] = magic_word.to_bytes(U32_SIZE_BYTES, 'little')

        pos += U32_SIZE_BYTES
        fpt_data.insert(pos, fpt_version)

        pos += U8_SIZE_BYTES
        fpt_data.insert(pos, fpt_header_size)

        pos += U8_SIZE_BYTES
        fpt_data.insert(pos, fpt_entry_size)

        pos += U8_SIZE_BYTES
        fpt_data.insert(pos, num_entries)

        index_tuple = 0
        for fpt_tuple in fpt_entry_list:
            # FPT entry type
            pos = fpt_entry_offset * (index_tuple + 1)
            fpt_data[pos:0] = fpt_tuple.type.to_bytes(U32_SIZE_BYTES, 'little')

            # FPT entry base address
            pos += U32_SIZE_BYTES
            fpt_data[pos:0] = fpt_tuple.base_addr.to_bytes(U32_SIZE_BYTES, 'little')

            # FPT entry partition size
            pos += U32_SIZE_BYTES
            fpt_data[pos:0] =  fpt_tuple.partition_size.to_bytes(U32_SIZE_BYTES, 'little')

            # FPT entry partition flag
            pos += U32_SIZE_BYTES
            fpt_data[pos:0] = fpt_tuple.partition_flags.to_bytes(U32_SIZE_BYTES, 'little')

            index_tuple += 1

        fpt_data = fpt_data[:fpt_size]
        if args.verbose:
            # dump out the generated FPT
            print(hexdump(fpt_data))
    except Exception as e:
        print('Error: Failed to populate FPT data array: ' + str(e))
        raise SystemExit(1)

    # Step6: Write bytearray to binary file
    try:
        fpt_bin_file = os.path.join(args.output_path, "amr_fpt.bin")
        print('FPT file: ' + fpt_bin_file)

        with open(fpt_bin_file, 'wb') as fp:
            fp.write(fpt_data)
            print('Successfully generated binary amr_fpt.bin...')
    except Exception as e:
        print(e)

if __name__ == '__main__':
    main()
