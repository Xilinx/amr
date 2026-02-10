#!/usr/bin/env python3

# Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# E.g. Build PDI with FPT:
#./scripts/fpt_pdi_gen.py -p <rave|v80> -i <OSPI.pdi> -o <OSPI_fpt.bin>
#
# This will create a OSPI pdi/bin with Flash Partition Table (FPT)
#

import sys
import os
import hashlib
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

fpt_config_data = {
    'fpt_header': {
        'magic_word':       0x92F7A516,
        'fpt_version':      2,
        'fpt_header_size':  128,
        'fpt_entry_size':   128,
        'num_entries':      3,
        'fpt_entry_offset': 128 * 1024,           # 0x0002_0000
    },
    'fpt_entry_rave': [
        {
            'type':            0x00000E00,        # "PDI"
            'base_addr':       0x00080000,
            'partition_size':  58 * 1024 * 1024,  # 0x03A0_0000
            'pdi_md5':         0x00000000000000000000000000000000,
            'pdi_size':        0x00000000,
            'partition_flags': 0x00,
        },
        {
            'type':            0x00000E00,        # "PDI"
            'base_addr':       0x03B80000,
            'partition_size':  58 * 1024 * 1024,  # 0x03A0_0000
            'pdi_md5':         0x00000000000000000000000000000000,
            'pdi_size':        0x00000000,
            'partition_flags': 0x00,
        },
        {
            'type':            0x00000F00,        # "USER"
            'base_addr':       0x07680000,
            'partition_size':  8 * 1024 * 1024,   # 0x0080_0000
            'pdi_md5':         0x00000000000000000000000000000000,
            'pdi_size':        0x00000000,
            'partition_flags': 0x00,
        },
    ],
    'fpt_entry_v80': [
        {
            'type':            0x00000E00,        # "PDI"
            'base_addr':       0x00080000,
            'partition_size':  116 * 1024 * 1024, # 0x0740_0000
            'pdi_md5':         0x00000000000000000000000000000000,
            'pdi_size':        0x00000000,
            'partition_flags': 0x00,
        },
        {
            'type':            0x00000E00,        # "PDI"
            'base_addr':       0x07480000,
            'partition_size':  116 * 1024 * 1024, # 0x0740_0000
            'pdi_md5':         0x00000000000000000000000000000000,
            'pdi_size':        0x00000000,
            'partition_flags': 0x00,
        },
        {
            'type':            0x00000F00,        # "USER"
            'base_addr':       0x0E880000,
            'partition_size':  16 * 1024 * 1024,  # 0x0170_0000
            'pdi_md5':         0x00000000000000000000000000000000,
            'pdi_size':        0x00000000,
            'partition_flags': 0x00,
        },
    ],
}

# Constants
MD5_SIZE_BYTES = 16
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

# Function to print an error message and exit
def error_exit(msg):
    print("ERROR: " + msg)
    exit(1)

# Function to dump data to a file
def write_data_to_file(filename, data, permissions='w+'):
    try:
        f = open(filename, permissions)
        f.write(data)
        f.close()
    except:
        error_exit(str(sys.exc_info()[1]))

# Function to read data from a file
def read_data_from_file(filename, permissions='r'):
    try:
        f = open(filename, permissions)
        data = f.read()
        file_size = os.path.getsize(filename)
        md5_hash = hashlib.md5(data).hexdigest()
        f.close()
    except:
        error_exit(str(sys.exc_info()[1]))

    return data, md5_hash, file_size

# Function to generate FPT binary file
def do_generate_fpt(profile, md5_hash, file_size, verbose):

    # Step1: Print verbose args
    if verbose:
        print('profile:     ' + profile)
        print('MD5 hash:    ' + md5_hash)
        print('File size:   ' + file_size)

    # Step2: load the FPT configuration data
    data = fpt_config_data

    # Step3: Parse the FPT header
    magic_word       = data['fpt_header']['magic_word']
    fpt_version      = data['fpt_header']['fpt_version']
    fpt_header_size  = data['fpt_header']['fpt_header_size']
    fpt_entry_size   = data['fpt_header']['fpt_entry_size']
    num_entries      = data['fpt_header']['num_entries']
    fpt_entry_offset = data['fpt_header']['fpt_entry_offset']

    # Step4: Parse the FPT entries
    data['fpt_entry_' + profile][0]['pdi_size'] = int(file_size)
    data['fpt_entry_' + profile][0]['pdi_md5']  = int(md5_hash, 16)

    fpt_entry = namedtuple('fpt_entry', 'type base_addr partition_size pdi_md5 pdi_size partition_flags')
    fpt_entry_list = []
    for entry in data['fpt_entry_' + profile]:
        fpt_entry_list.append(fpt_entry(
                              entry.get('type'),
                              entry.get('base_addr'),
                              entry.get('partition_size'),
                              entry.get('pdi_md5'),
                              entry.get('pdi_size'),
                              entry.get('partition_flags')))
    if verbose:
        print('\nFPT Header:')
        print('    magic_word:      ', hex(magic_word))
        print('    fpt_version:     ', fpt_version)
        print('    fpt_header_size: ', fpt_header_size)
        print('    fpt_entry_size:  ', fpt_entry_size)
        print('    num_entries:     ', num_entries)
        print('    fpt_entry_offset:', hex(fpt_entry_offset))
        for i, fpt_tuple in enumerate(fpt_entry_list):
            print('FPT entry:',        i)
            print('  type:            ', hex(fpt_tuple.type))
            print('  base_addr:       ', hex(fpt_tuple.base_addr))
            print('  partition_size:  ', hex(fpt_tuple.partition_size))
            print('  pdi_md5:         ', hex(fpt_tuple.pdi_md5))
            print('  pdi_size:        ', hex(fpt_tuple.pdi_size))
            print('  partition_flags: ', hex(fpt_tuple.partition_flags))
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

            # FPT entry pdi md5
            pos += U32_SIZE_BYTES
            fpt_data[pos:0] = fpt_tuple.pdi_md5.to_bytes(MD5_SIZE_BYTES, 'big')

            # FPT entry pdi size
            pos += MD5_SIZE_BYTES
            fpt_data[pos:0] = fpt_tuple.pdi_size.to_bytes(U32_SIZE_BYTES, 'little')

            # FPT entry partition flags (user defined flags)
            pos += U32_SIZE_BYTES
            fpt_data[pos:0] = fpt_tuple.partition_flags.to_bytes(U32_SIZE_BYTES, 'little')

            index_tuple += 1

        fpt_data = fpt_data[:fpt_size]
        if verbose:
            # dump out the generated FPT
            print(hexdump(fpt_data))
    except Exception as e:
        print('Error: Failed to populate FPT data array: ' + str(e))
        raise SystemExit(1)

    return fpt_data

# Function to generate FPT Setup PDI
def do_generate_fpt_pdi(profile, pdi_file, output_file_name, verbose):

    # open the PDI and create FPT + PDI structure
    if pdi_file.endswith(('.pdi', '.bin')):
        pdi_data, pdi_md5_hash, pdi_file_size = read_data_from_file(pdi_file, 'rb')
        pdi_bin_data = bytearray(pdi_data)
    else:
        error_exit("PDI file must be either be *.bin or *.pdi suffix - {}".format(
            pdi_file))

    # Generate FPT binary file
    fpt_bin_data = do_generate_fpt(profile, str(pdi_md5_hash), str(pdi_file_size), verbose)

    # pad the binary file to 32KB to align to PMC boot search
    fpt_bin_data = fpt_bin_data.ljust(0x8000, b'\xff')

    # combine FPT and PDI data
    combined_bin_data = fpt_bin_data + pdi_bin_data

    # write the combined data to the output file
    write_data_to_file(output_file_name, combined_bin_data, 'wb')


# Script main entry point
if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='Generate AMR FPT Setup PDI')
    parser.add_argument('-p', '--profile',     help='profile [rave|v80]', default = 'rave')
    parser.add_argument('-i', '--pdi', dest='pdi_file', metavar=('pdi_file'),
                        help='PDI File, to be combined with FPT')
    parser.add_argument('-o', '--output', dest='outfile', default='-', metavar=('output_file'),
                        help='Destination file after an input file(s) processed')
    parser.add_argument('-v', '--verbose',     action='store_true', help="increase output verbosity")

    # If nothing is input to this script, print usage
    if len(sys.argv[1:]) == 0:
        parser.print_help()
        parser.exit()

    args = parser.parse_args()

    # Generate FPT Setup PDI

    # Both a PDI and FPT is required for this option
    if not args.profile or not args.pdi_file :
        error_exit("FPT Setup PDI Generation requires --profile and --pdi to be specified")

    if args.outfile == '-':
        out_file_local = "./build/amr_ospi_fpt.bin"
    elif False == args.outfile.lower().endswith(('.bin','.pdi')):
        error_exit("Please provide an output filename with suffix '.pdi' or '.bin'")
    else:
        out_file_local = args.outfile


    print("Generating FPT Setup PDI :\t\t{}".format(out_file_local))
    do_generate_fpt_pdi(args.profile, args.pdi_file, out_file_local, args.verbose)
    print("FPT Setup PDI Generated ****************************************")
    sys.exit()
