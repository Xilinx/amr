#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

import re
import os
import sys
import tarfile

from os import walk
from os.path import join
from os.path import abspath

SCRIPT_VERSION = '1.0'
SCRIPT_FILE    = os.path.basename(__file__)
SCRIPT_DIR     = os.path.dirname(os.path.realpath(__file__))
PROJECT_DIR    = abspath(join(SCRIPT_DIR, os.pardir))
BUILD_DIR      = abspath(join(SCRIPT_DIR, 'pkg_data'))
sys.path.insert(0, BUILD_DIR)

from pkg import *

# Get date
build_date       = get_date_long()
build_date_short = get_date_short()
now              = datetime.datetime.now()

class Options(object):
    def PrintVersion(self):
        log_info('GPKG-05', 'Script ' + SCRIPT_FILE + ' version: ' + SCRIPT_VERSION)

    def printHelp(self):
        log_info('GPKG-06', 'Usage: $ python3 ' + SCRIPT_FILE + ' [options]')
        log_info('GPKG-06', '\t--help             / -h: Display this message')
        log_info('GPKG-06', '\t--output_dir       / -o: Path to the output directory. Default: ./output/<date>_<time>')
        log_info('GPKG-06', '\t--pkg_release      / -r: Package release. Default <date>')
        log_info('GPKG-06', '\t--verbose          / -V: Turn on verbosity')
        log_info('GPKG-06', '\t--force            / -f: Override output directory if already existing')
        log_info('GPKG-06', '\t--version          / -v: Display version')
        log_info('GPKG-06', '\t--no_gen_version   / -g: No genVersion scripts run during gen_package - docker build option only')
        log_info('GPKG-06', '')

    def __init__(self):
        self.help = False
        self.output_dir = None
        self.pkg_release = None
        self.force = False
        self.verbose = False
        self.version = False
        self.no_gen_version = False

    def getOptions(self, argv):
        log_info('GPKG-62', 'Command line provided: $ ' + str(sys.executable) + ' ' + ' '.join(argv))
        try:
            options, remainder = getopt.gnu_getopt(
                argv[1:],
                'ho:r:Vfvng',
                [
                    'help',
                    'output_dir=',
                    'pkg_release=',
                    'verbose',
                    'force',
                    'version',
                    'no_gen_version'
                ]
            )
        except getopt.GetoptError as e:
            self.printHelp()
            exit_error('GPKG-01', str(e))

        log_info('GPKG-63', 'Parsing command line options')

        for opt, arg in options:
            msg = '\t' + str(opt)
            if arg is not None:
                msg += ' ' + str(arg)
            log_info('GPKG-63', msg)

            if opt in ('--help', '-h'):
                self.printHelp()
                self.help = True
            elif opt in ('--output_dir', '-o'):
                self.output_dir = str(arg)
                self.output_dir = abspath(self.output_dir)
            elif opt in ('--pkg_release', '-r'):
                self.pkg_release = str(arg)
            elif opt in ('--verbose', '-V'):
                setup_verbose()
                self.verbose = True
            elif opt in ('--force', '-f'):
                self.force = True
            elif opt in ('--version', '-v'):
                self.PrintVersion()
                self.version = True
            elif opt in ('--no_gen_version', '-g'):
                self.no_gen_version = True
            else:
                exit_error('GPKG-02', 'Command line option not handled: ' + str(opt))

        if len(remainder) > 0:
            self.printHelp()
            exit_error('GPKG-03', 'Unknown command line options: ' + ' '.join(remainder))

        if self.help or self.version:
            exit_info('GPKG-04', 'Script terminating as help/version option provided')

        if self.pkg_release is None:
            self.pkg_release = str(now.year) + str(now.month).zfill(2) + str(now.day).zfill(2)

def main(args):
    # Parse command line options
    opt = Options()
    opt.getOptions(args)

    config = {}

    # Driver and API sources - will look for them later, after we run
    # the getVersion.sh script (will miss the version headers otherwise)
    driver = []
    api_headers = []

    try:
        script_start_time = start('GPKG-07', SCRIPT_FILE)

        ##############################

        # Get/define output directory
        if opt.output_dir is not None:
            output_dir = opt.output_dir
        else:
            output_dir =        str(now.year)          + '-' + str(now.month).zfill(2)  + '-' + str(now.day).zfill(2)
            output_dir += '_' + str(now.hour).zfill(2) + '-' + str(now.minute).zfill(2) + '-' + str(now.second).zfill(2)
            output_dir = abspath(join(CWD, 'output', output_dir))

        # Define steps output directories and others
        tmp_dir        = abspath(join(output_dir, 'tmp'))
        log_dir        = abspath(join(output_dir, 'log'))
        bkp_design_dir = abspath(join(output_dir, 'bkp_design'))

        # Create output directory
        if os.path.isdir(output_dir):
           if not opt.force:
               exit_error('GPKG-17', 'Output directory already exists (see --force to override): ' + output_dir)
           else:
               log_info('GPKG-60', 'Removing output directory already existing as --force option is provided: ' + output_dir)
               force_remove_dir('GPKG-61', output_dir)

        log_info('GPKG-18', 'Creating output directory: ' + output_dir + '\n')
        os.makedirs(output_dir)

        if not os.path.isdir(log_dir):
            os.makedirs(log_dir)

        setup_logfile(abspath(join(log_dir, os.path.splitext(SCRIPT_FILE)[0] + '.log')))

        if not os.path.isdir(tmp_dir):
            os.makedirs(tmp_dir)

        if not os.path.isdir(bkp_design_dir):
            os.makedirs(bkp_design_dir)

        ##############################

        step       = 'get system info'
        start_time = start_step('GPKG-39', step)

        # Get System info
        config['system'] = {}

        # Get config['system']['distribution_id']
        cmd = ['lsb_release', '-is']
        log_file_name = abspath(join(output_dir, 'lsb_release_is.log'))
        log_info('GPKG-05', 'Get distribution ID')
        exec_step_cmd('GPKG-15', step, cmd, log_file_name)
        check_file_exists('GPKG-05', log_file_name)

        log_file = open(log_file_name, mode='r')
        for line in log_file:
            config['system']['dist_id'] = line.split('\n')[0]
            break
        log_file.close()

        if config['system']['dist_id'] not in SUPPORTED_DIST_ID:
            exit_error('GPKG-14', 'Invalid Distribution ID: ' + config['system']['dist_id'] + '. Supported values are ' + str(SUPPORTED_DIST_ID))

        log_info('GPKG-16', 'Current distribution ID: ' + config['system']['dist_id'])

        # Get config['system']['distribution_release']
        cmd = ['lsb_release', '-rs']
        log_file_name = abspath(join(output_dir, 'lsb_release_rs.log'))
        exec_step_cmd('GPKG-15', step, cmd, log_file_name)
        log_file = open(log_file_name, mode='r')
        for line in log_file:
            config['system']['dist_rel'] = line.split('\n')[0]
            break
        log_file.close()

        # architecture
        if config['system']['dist_id'] in DIST_DEB:
            cmd = ['dpkg', '--print-architecture']
            log_file_name = abspath(join(output_dir, 'dpkg_print_arch.log'))
            exec_step_cmd('GPKG-15', step, cmd, log_file_name)
            log_file = open(log_file_name, mode='r')

            for line in log_file:
                config['system']['arch'] = line.split('\n')[0]
                break
            log_file.close()
        else:
            config['system']['arch'] = platform.machine() # 'x86_64'

        if config['system']['arch'] not in SUPPORTED_ARCH:
            exit_error('GPKG-17', 'Invalid architecture: ' + config['system']['arch'] + '. Supported values are ' + str(SUPPORTED_ARCH))

        log_info('GPKG-18', 'System:')
        log_info('GPKG-18', '\t - Distribution ID: '      + config['system']['dist_id'])
        log_info('GPKG-18', '\t - Distribution release: ' + config['system']['dist_rel'])
        log_info('GPKG-18', '\t - Architecture: '         + config['system']['arch'])

        end_step('GPKG-38', start_time)

        ##############################

        # Init
        config['vendor']          = {}
        config['vendor']['name']  = 'xilinx'
        config['vendor']['full']  = 'Xilinx Inc'
        config['vendor']['email'] = 'support@xilinx.com'

        # for AMI build as part of amr_build flow - do not run genVersion for AMI or GCQ - these are run in advance
        if not opt.no_gen_version :
            # Get package version
            step = 'get AMI version'
            start_time = start_step('GET_VER', step)
            get_ver = './scripts/getVersion.sh ami'
            exec_step_cmd('GEN_VERSION', step, get_ver, shell=True, cwd=PROJECT_DIR)
            check_file_exists('GET_VER', join(PROJECT_DIR, 'api', 'include', 'ami_version.h.in'))
            end_step('GET_VER', start_time)

        # When building the list of sources, we need a relative path so we split
        # on PROJECT_DIR, then split once more to remove the leading slash, e.g.
        # PROJECT_DIR/driver/foo.c -> /driver/foo.c -> driver/foo.c

        config['pkg']              =  {}
        config['pkg']['name']      =  'amitool'
        config['pkg']['release']   =  opt.pkg_release
        config['pkg']['summary']   =  config['pkg']['name'] + ' package'
        config['pkg']['changelog'] =  config['pkg']['name'] + ' package. Built on $build_date_short.'
        config['pkg']['descr']     = [config['pkg']['name'] + ' package', 'Built on ' + build_date_short + '.']

        # Find version from generated header file
        with open(join(PROJECT_DIR, 'api', 'build', 'ami_version.h'), 'r') as fd:
                data = fd.read()

                v = re.findall(r'GIT_TAG.*?\"(\d+\.\d+\.\d+).*\"$', data, re.M)
                c = re.findall(r'GIT_TAG_VER_DEV_COMMITS.*?\((\d+)\)$', data, re.M)
                h = re.findall(r'GIT_HASH.*?\"(.*?)\"$', data, re.M)

                # Set version
                config['pkg']['version'] = v[0] if v else '0.0.0'

                # Set release
                config['pkg']['release'] = f'{c[0] if c else 0}.{h[0][:8] if h else ""}.{opt.pkg_release}'

        # preinst.sh
        with open(abspath(join(SCRIPT_DIR, 'pkg_data', 'preinst.sh')), 'r') as infile:
            fdata = infile.read()
            fdata = fdata.replace('#!/bin/sh',                '')
            fdata = fdata.replace('MODULE_NAME=$1',           'MODULE_NAME='+ config['pkg']['name'])
            fdata = fdata.replace('MODULE_VERSION_STRING=$2', 'MODULE_VERSION_STRING='+ config['pkg']['version'])
            config['pkg']['preinst'] = fdata.split('\n')
            with open(abspath(join(output_dir, 'preinst.sh')), 'w') as outfile:
                outfile.write('\n'.join(config['pkg']['preinst']))

        config['pkg']['deps'] = {
            'rpm': ['glibc', 'grep', 'gawk'],
            'deb': ['libc6', 'grep', 'gawk']
        }

        # We want to conflict with XRT, however, we don't want the package manager to automatically
        # remove XRT and any packages that depend on it. This is handled correctly by `yum`` with RPM files,
        # but `apt`` tries to automatically uninstall XRT if it is detected. To get around this, we use a
        # 'preinst' scriptlet rather than the 'Conflicts' field in the control file.
        config['pkg']['conflicts'] = {}
        config['pkg']['conflicts']['rpm'] = ['xrt']
        config['pkg']['conflicts']['deb'] = []

        # Build libami.so and ami_tool
        step = 'build AMR library and ami tool'
        start_time = start_step('BUILD_AMI', step)

        build_api = 'cd api && make clean && make'
        exec_step_cmd('BUILD_LIBAMI', step, build_api, shell=True, cwd=PROJECT_DIR)
        check_file_exists('BUILD_LIBAMI', join(PROJECT_DIR, 'api', 'build', 'libami.so'))

        build_ami_tool = 'cd app && make clean && make PROFILE=RAVE'
        exec_step_cmd('BUILD_AMI_TOOL', step, build_ami_tool, shell=True, cwd=PROJECT_DIR)
        check_file_exists('BUILD_AMI_TOOL', join(PROJECT_DIR, 'app', 'build', 'ami_tool'))

        end_step('BUILD_AMI', start_time)

        # Define package content paths
        config['pkg']['usr_bin_dir']       = 'usr/local/bin/'
        config['pkg']['usr_bin']           = config['pkg']['usr_bin_dir'] + '/ami_tool'


        if config['system']['dist_id'] in DIST_DEB:
            config['pkg']['pkg_config_dir'] = 'usr/lib/pkgconfig'
        else:
            config['pkg']['pkg_config_dir'] = 'usr/share/pkgconfig'
        config['pkg']['pkg_config_pc']  = config['pkg']['pkg_config_dir'] + '/ami_tool.pc'

        # Define package description metadata
        config['pkg']['descr']  = [
            'Xilinx Inc ' + config['pkg']['name'] + ' package.',
            'Built on '   + str(build_date_short) + '.',
            'Built with ' + config['system']['dist_id'] + ' version ' + config['system']['dist_rel'] + ' and architecture ' + config['system']['arch'] + '.',
        ]

        # Create the file necessary to generate the packages
        config['pkg']['descr'] = '\n '.join(config['pkg']['descr']); # DEB package format requires \n+SPACE
        deb_name = config['pkg']['name'] + '_' + config['pkg']['version'] + '-' + config['pkg']['release'] + '_' + config['system']['arch']
        dest_base  = abspath(join(output_dir, deb_name))
        debian_dir = abspath(join(dest_base, 'DEBIAN'))
        os.makedirs(dest_base)
        os.chmod(dest_base, 493); # octal 0755
        os.makedirs(debian_dir)
        check_dir_exists('GPKG-05', dest_base)
        check_dir_exists('GPKG-05', debian_dir)
        # Create control file
        CONTROL = []
        CONTROL += ['Package: '        + config['pkg']['name']]
        CONTROL += ['Architecture: '   + config['system']['arch']]
        CONTROL += ['Version: '        + config['pkg']['version'] + '-' + config['pkg']['release']]
        CONTROL += ['Priority: optional']
        CONTROL += ['Description: '    + config['pkg']['descr']]
        CONTROL += ['Maintainer: '     + config['vendor']['full']]
        CONTROL += ['Section: devel']
        if len(config['pkg']['deps']['deb']) > 0:
            CONTROL += ['Depends: ' + ', '.join(config['pkg']['deps']['deb'])]
        if len(config['pkg']['conflicts']['deb']) > 0:
            CONTROL += ['Conflicts: ' + ', '.join(config['pkg']['conflicts']['deb'])]
        CONTROL += ['']
        control_file_name = abspath(join(debian_dir, 'control'))
        log_info('GPKG-26', 'Writing control file:   ' + control_file_name)
        with open(control_file_name, mode='w') as outfile:
            outfile.write('\n'.join(CONTROL))
        check_file_exists('GPKG-05', control_file_name)
        
         # Create preinst file
        PREINST = [
            '#!/bin/bash',
            'set -e',
        ]
        PREINST += config['pkg']['preinst']
        preinst_file_name = abspath(join(debian_dir, 'preinst'))
        log_info('GPKG-28', 'Writing preinst file:   ' + preinst_file_name)
        with open(preinst_file_name, mode='w') as outfile:
            outfile.write('\n'.join(PREINST))
        os.chmod(preinst_file_name, 509); # octal 775
        check_file_exists('GPKG-05', preinst_file_name)
        # Create changelog
        CHANGE_LOG = []
        CHANGE_LOG += ['']
        CHANGE_LOG += [config['pkg']['name'] + ' (' + config['pkg']['version'] + '-' + config['pkg']['release'] + ') xilinx; urgency=medium']
        CHANGE_LOG += ['']
        CHANGE_LOG += ['  * ' + config['pkg']['changelog']]
        CHANGE_LOG += ['']
        CHANGE_LOG += ['-- ' + config['vendor']['full']+' <' + config['vendor']['email'] + '> ' + build_date_short + ' 00:00:00 +0000']
        CHANGE_LOG += ['']
        changelog_dir       = abspath(join(dest_base, 'usr', 'share', 'doc', config['pkg']['name']))
        changelog_file_name = abspath(join(changelog_dir, 'changelog.Debian'))
        changelog_tar_name  = abspath(join(changelog_dir, 'changelog.Debian.gz'))
        os.makedirs(changelog_dir)
        log_info('GPKG-29', 'Writing changelog file: ' + changelog_file_name)
        with open(changelog_file_name, mode='w') as outfile:
            outfile.write('\n'.join(CHANGE_LOG))
        with tarfile.open(changelog_tar_name, "w:gz") as tar:
            tar.add(changelog_file_name)
        os.remove(changelog_file_name)
        check_file_exists('GPKG-05', changelog_tar_name)
        # Copying packaged files
        SRC_DEST_LIST = [
            {'src': abspath(join(PROJECT_DIR, 'app',  'build', 'ami_tool' )),'dst': config['pkg']['usr_bin_dir']},
        ]

        for src_dest in SRC_DEST_LIST:
            src = src_dest['src']
            dst = dest_base
            if src_dest['dst'] != '':
                dst = abspath(join(dst, src_dest['dst']))
            copy_source_file('GPKG-31', src, dst)
            check_file_exists('GPKG-05', abspath(join(dst, os.path.basename(src))))

            # Replace version in dkms.conf
            if src.endswith('dkms.conf'):
                with open(src, 'r') as dkms_src:
                    conf = dkms_src.read()
                    with open(join(dst, os.path.basename(src)), 'w') as dkms_dest:
                        new_conf = conf.replace('@PKGVER@', config['pkg']['version'])
                        dkms_dest.write(new_conf)

        # Create module PC file
        MODULE_PC = [
            'bindir=/'      + config['pkg']['usr_bin_dir'],
            '',
            'Name: '        + config['pkg']['name'],
            'Description: ' + config['pkg']['summary'],
            'Version: '     + config['pkg']['version'],
        ]
        os.makedirs(abspath(join(dest_base, config['pkg']['pkg_config_dir'])))
        module_pc_file_name = abspath(join(dest_base, config['pkg']['pkg_config_pc']))
        log_info('GPKG-28', 'Writing module PC file: ' + module_pc_file_name)
        with open(module_pc_file_name, mode='w') as outfile:
            outfile.write('\n'.join(MODULE_PC))
        check_file_exists('GPKG-05', module_pc_file_name)

        # Generate package
        cmd = ['dpkg-deb', '--build', '--root-owner-group', join(output_dir, deb_name)]
        log_file_name = abspath(join(log_dir, 'dpkg-deb.log'))

        pkg_name = config['pkg']['name'] + '_' + config['pkg']['version'] + '-' + config['pkg']['release'] + '_' + config['system']['arch']
        pkg     = abspath(join(output_dir, pkg_name + '.deb'))
        pkg_cpy = abspath(join(output_dir, pkg_name + '_' + config['system']['dist_rel'] + '.deb'))

        log_newline()
        log_info('GPKG-37', 'Building package...')
        exec_step_cmd('GPKG-37', step, cmd, log_file_name)
        check_file_exists('GPKG-05', pkg)
        shutil.copyfile(pkg, pkg_cpy)
        os.remove(pkg)
        check_file_exists('GPKG-05', pkg_cpy)

        end_step('GPKG-38', start_time)

        tear_down('GPKG-8', SCRIPT_FILE, script_start_time)

    except OSError as o:
        print(o)
        raise RuntimeError(o.errno)
    except AssertionError as a:
        print(a)
        raise RuntimeError(1)
    except Exception as e:
        print(e)
        raise RuntimeError(1)
    finally:
        print('')

    sys.exit(1)

if __name__ == '__main__':
    main(sys.argv)
