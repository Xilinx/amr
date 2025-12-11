#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

import re
import os
import sys
import tarfile
import platform

from os import walk
from os.path import join
from os.path import abspath

SCRIPT_VERSION = '1.1'
SCRIPT_FILE    = os.path.basename(__file__)
SCRIPT_DIR     = os.path.dirname(os.path.realpath(__file__))
PROJECT_DIR    = abspath(join(SCRIPT_DIR, os.pardir))
BUILD_DIR      = abspath(join(SCRIPT_DIR, 'pkg_data'))
sys.path.insert(0, BUILD_DIR)

from pkg import *

# Get date
build_date = get_date_str()
now        = datetime.datetime.now()

class Options(object):
    def PrintVersion(self):
        log_info('GPKG-05', 'Script ' + SCRIPT_FILE + ' version: ' + SCRIPT_VERSION)

    def printHelp(self):
        log_info('GPKG-06', 'Usage: $ python3 ' + SCRIPT_FILE + ' [options]')
        log_info('GPKG-06', '\t--help           / -h: Display this message')
        log_info('GPKG-06', '\t--out_dir        / -o: Path to the output directory. Default: ./output/<date>_<time>')
        log_info('GPKG-06', '\t--pkg_release    / -r: Package release. Default <date>')
        log_info('GPKG-06', '\t--verbose        / -V: Turn on verbosity')
        log_info('GPKG-06', '\t--force          / -f: Override output directory if already existing')
        log_info('GPKG-06', '\t--version        / -v: Display version')
        log_info('GPKG-06', '')

    def __init__(self):
        self.help = False
        self.out_dir = None
        self.pkg_release = None
        self.force = False
        self.verbose = False
        self.version = False

    def getOptions(self, argv):
        log_info('GPKG-62', 'Command line provided: $ ' + str(sys.executable) + ' ' + ' '.join(argv))
        try:
            options, remainder = getopt.gnu_getopt(
                argv[1:],
                'ho:r:Vfv',
                [
                    'help',
                    'out_dir=',
                    'pkg_release=',
                    'verbose',
                    'force',
                    'version'
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
            elif opt in ('--out_dir', '-o'):
                self.out_dir = str(arg)
                self.out_dir = abspath(self.out_dir)
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

    # Driver sources - will look for them later, after we run
    driver = []

    try:
        script_start_time = start('GPKG-07', SCRIPT_FILE)

        ##############################

        # Get/define output directory
        if opt.out_dir is not None:
            out_dir = opt.out_dir
        else:
            out_dir =        str(now.year)          + '-' + str(now.month).zfill(2)  + '-' + str(now.day).zfill(2)
            out_dir += '_' + str(now.hour).zfill(2) + '-' + str(now.minute).zfill(2) + '-' + str(now.second).zfill(2)
            out_dir = abspath(join(CWD, 'output', out_dir))

        # Define steps output directories and others
        log_dir = abspath(join(out_dir, 'log'))

        # Create output directory
        if os.path.isdir(out_dir):
           if not opt.force:
               exit_error('GPKG-17', 'Output directory already exists (see --force to override): ' + out_dir)
           else:
               log_info('GPKG-60', 'Removing output directory already existing as --force option is provided: ' + out_dir)
               force_remove_dir('GPKG-61', out_dir)

        log_info('GPKG-18', 'Creating output directory: ' + out_dir + '\n')
        os.makedirs(out_dir)

        if not os.path.isdir(log_dir):
            os.makedirs(log_dir)

        setup_logfile(abspath(join(log_dir, os.path.splitext(SCRIPT_FILE)[0] + '.log')))

        ##############################

        step       = 'get system info'
        start_time = start_step('GPKG-39', step)

        # Get System info
        config['system'] = {}

        # Get config['system']['dist_id']
        cmd = ['lsb_release', '-is']
        log_file_name = abspath(join(out_dir, 'lsb_release_is.log'))
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

        # Get config['system']['dist_rel']
        cmd = ['lsb_release', '-rs']
        log_file_name = abspath(join(out_dir, 'lsb_release_rs.log'))
        exec_step_cmd('GPKG-15', step, cmd, log_file_name)
        log_file = open(log_file_name, mode='r')
        for line in log_file:
            config['system']['dist_rel'] = line.split('\n')[0]
            break
        log_file.close()

        # Only use the major release number of CentOS and RedHat
        if config['system']['dist_id'].lower() in [d.lower() for d in  [DIST_ID_CENTOS, DIST_ID_REDHAT, DIST_ID_REDHAT2, DIST_ID_RHEL]]:
            dist_rel_split = config['system']['dist_rel'].split('.')
            config['system']['dist_rel'] = dist_rel_split[0] + '.x'

        # architecture
        if config['system']['dist_id'] in DIST_DEB:
            cmd = ['dpkg', '--print-architecture']
            log_file_name = abspath(join(out_dir, 'dpkg_print_arch.log'))
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

        # When building the list of sources, we need a relative path so we split
        # on PROJECT_DIR, then split once more to remove the leading slash, e.g.
        # PROJECT_DIR/driver/foo.c -> /driver/foo.c -> driver/foo.c

        # for AMI build as part of aved_build flow - do not run driver generation within docker flow
        # Check that the driver will build
        step = 'driver compilation confidence check'
        start_time = start_step('BUILD_DRIVER', step)

        build_driver = 'cd driver && make clean && make'
        exec_step_cmd('BUILD_CHECK_DRIVER', step, build_driver, shell=True, cwd=PROJECT_DIR)
        check_file_exists('BUILD_CHECK_DRIVER', join(PROJECT_DIR, 'driver', 'ami.ko'))

        step = 'clean driver'
        clean_driver = 'cd driver && make clean'
        exec_step_cmd('CLEAN_DRIVER', step, clean_driver, shell=True, cwd=PROJECT_DIR)

        end_step('BUILD_DRIVER', start_time)
        # Find driver sources
        for path, _, files in walk(join(PROJECT_DIR, 'driver')):

            for name in files:
                if name.endswith(('.c', '.h', 'Makefile')):
	                driver.append(join(path, name).split(PROJECT_DIR)[-1].split('/', 1)[-1])

        config['pkg']              =  {}
        config['pkg']['name']      =  'ami'
        config['pkg']['release']   =  opt.pkg_release
        config['pkg']['summary']   =  config['pkg']['name'] + ' driver package'
        config['pkg']['changelog'] =  config['pkg']['name'] + ' driver package. Built on $build_date.'
        config['pkg']['descr']     = [config['pkg']['name'] + ' driver package', 'Built on ' + build_date + '.']

        # Find version from generated header file
        with open(join(PROJECT_DIR, 'driver', 'ami_top.h'), 'r') as fd:
            data = fd.read()

            v = re.findall(r'AMI_VER.*?\"(\d+\.\d+\.\d+).*\"$', data, re.M)
            h, git_date = get_git_info('GPKG-05')

            # Set version
            config['pkg']['version'] = v[0] if v else '0.0.0'

            # Set release
            config['pkg']['release'] = f'{0}.{h[:8] if h else ""}.{opt.pkg_release}'

        # prerm.sh
        with open(abspath(join(SCRIPT_DIR, 'pkg_data', 'prerm.sh')), 'r') as infile:
            fdata = infile.read()
            fdata = fdata.replace('MOD_NAME="$1"',    'MOD_NAME='+ config['pkg']['name'])
            fdata = fdata.replace('MOD_VER_STR="$2"', 'MOD_VER_STR='+ config['pkg']['version'])
            config['pkg']['prerm'] = fdata.split('\n')
            with open(abspath(join(out_dir, 'prerm.sh')), 'w') as outfile:
                outfile.write('\n'.join(config['pkg']['prerm']))

        # preinst.sh
        with open(abspath(join(SCRIPT_DIR, 'pkg_data', 'preinst.sh')), 'r') as infile:
            fdata = infile.read()
            fdata = fdata.replace('MOD_NAME="$1"',    'MOD_NAME='+ config['pkg']['name'])
            fdata = fdata.replace('MOD_VER_STR="$2"', 'MOD_VER_STR='+ config['pkg']['version'])
            config['pkg']['preinst'] = fdata.split('\n')
            with open(abspath(join(out_dir, 'preinst.sh')), 'w') as outfile:
                outfile.write('\n'.join(config['pkg']['preinst']))

        # postinst.sh
        with open(abspath(join(SCRIPT_DIR, 'pkg_data', 'postinst.sh')), 'r') as infile:
            fdata = infile.read()
            fdata = fdata.replace('MOD_NAME="$1"',    'MOD_NAME='+ config['pkg']['name'])
            fdata = fdata.replace('MOD_VER_STR="$2"', 'MOD_VER_STR='+ config['pkg']['version'])
            config['pkg']['postinst'] = fdata.split('\n')
            with open(abspath(join(out_dir, 'postinst.sh')), 'w') as outfile:
                outfile.write('\n'.join(config['pkg']['postinst']))

        config['pkg']['deps'] = {
            'rpm': ['glibc', 'gcc', 'make', 'dkms', 'grep', 'gawk', 'kernel-devel', 'kernel-headers'],
            'deb': ['libc6', 'gcc', 'make', 'dkms', 'grep', 'gawk', 'linux-headers']
        }

        # We want to conflict with XRT, however, we don't want the package manager to automatically
        # remove XRT and any packages that depend on it. This is handled correctly by `yum`` with RPM files,
        # but `apt`` tries to automatically uninstall XRT if it is detected. To get around this, we use a
        # 'preinst' scriptlet rather than the 'Conflicts' field in the control file.
        config['pkg']['conflicts'] = {}
        config['pkg']['conflicts']['rpm'] = ['xrt']
        config['pkg']['conflicts']['deb'] = []

        # Define package content paths
        config['pkg']['usr_src_dir']       = 'usr/src/' + config['pkg']['name'] + '-' + config['pkg']['version']
        config['pkg']['usr_src_Makefile']  = config['pkg']['usr_src_dir'] + '/driver/Makefile'
        config['pkg']['usr_src_files']     = [f'{config["pkg"]["usr_src_dir"]}/{f}' for f in driver]
        config['pkg']['usr_src_dkms_conf'] = config['pkg']['usr_src_dir'] + '/dkms.conf'

        config['pkg']['usr_include_dir']   = 'usr/include/' + config['pkg']['name']

        config['pkg']['opt_dir']           = 'opt/amd/amr/amd_rave_gen3x4_25.1'


        if config['system']['dist_id'] in DIST_DEB:
            config['pkg']['pkg_cfg_dir'] = 'usr/lib/pkgconfig'
        else:
            config['pkg']['pkg_cfg_dir'] = 'usr/share/pkgconfig'
        config['pkg']['pkg_cfg_pc']  = config['pkg']['pkg_cfg_dir'] + '/ami.pc'

        # Define package description metadata
        config['pkg']['descr']  = [
            'Xilinx Inc ' + config['pkg']['name'] + ' driver package.',
            'Built on '   + str(build_date) + '.',
            'Built with ' + config['system']['dist_id'] + ' version ' + config['system']['dist_rel'] + ' and architecture ' + config['system']['arch'] + '.',
        ]

        # Create the file necessary to generate the packages
        if config['system']['dist_id'].lower() in [d.lower() for d in DIST_RPM]:
            for dirname in ['BUILDROOT', 'RPMS', 'SOURCES', 'SPECS', 'SRPMS', 'BUILD']:
                dir = os.path.abspath(os.path.join(out_dir, dirname))
                os.makedirs(dir)
                os.chmod(dir, 493); # octal 0755
                check_dir_exists('GPKG-5', dir)

            dest_base = os.path.abspath(os.path.join(out_dir, 'BUILD'))

            # Create specfile
            SPEC_FILE =  []
            SPEC_FILE += ['# Turn off the brp-python-bytecompile script']
            SPEC_FILE += ["%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*\$!!g')"]
            SPEC_FILE += ['']
            SPEC_FILE += ['Name: '                 + config['pkg']['name']]
            SPEC_FILE += ['Version: '              + config['pkg']['version']]
            SPEC_FILE += ['Release: '              + config['pkg']['release']]
            SPEC_FILE += ['Vendor: '               + config['vendor']['full']]
            SPEC_FILE += ['License: '              + config['vendor']['name']]
            SPEC_FILE += ['Summary: '              + config['pkg']['summary']]
            SPEC_FILE += ['BuildArchitectures: '   + config['system']['arch']]
            SPEC_FILE += ['Buildroot: %{_topdir}' ]
            for dep in config['pkg']['deps']['rpm']:
                SPEC_FILE += ['Requires: ' + dep]
            SPEC_FILE += ['']
            for conflict in config['pkg']['conflicts']['rpm']:
                SPEC_FILE += ['Conflicts: ' + conflict]
            SPEC_FILE += ['' ]
            SPEC_FILE += ['%description']
            SPEC_FILE += config['pkg']['descr']
            SPEC_FILE += ['']

            SPEC_FILE += ['%install']
            SPEC_FILE += ['mkdir -p %{buildroot}/' + config['pkg']['usr_src_dir'] + '/driver']
            SPEC_FILE += ['install -m 0644 ' + config['pkg']['usr_src_Makefile'] + ' %{buildroot}/' + config['pkg']['usr_src_Makefile']]

            dirs_done = []
            for file in config['pkg']['usr_src_files']:
                d = os.path.dirname(file)
                if d not in dirs_done:
                    SPEC_FILE += ['mkdir -p %{buildroot}/' + d]
                    dirs_done.append(d)

                SPEC_FILE += ['install -m 0644 ' + file + ' %{buildroot}/' + file]

            SPEC_FILE += ['install -m 0644 ' + config['pkg']['usr_src_dkms_conf'] + ' %{buildroot}/' + config['pkg']['usr_src_dkms_conf']]

            SPEC_FILE += ['mkdir -p %{buildroot}/' + config['pkg']['usr_include_dir']]

            SPEC_FILE += ['mkdir -p %{buildroot}/' + config['pkg']['pkg_cfg_dir']]
            SPEC_FILE += ['install -m 0644 ' + config['pkg']['pkg_cfg_pc'] + ' %{buildroot}/' + config['pkg']['pkg_cfg_pc']]

            SPEC_FILE += ['']

            SPEC_FILE += ['%posttrans']
            SPEC_FILE += config['pkg']['postinst']
            SPEC_FILE += ['']

            SPEC_FILE += ['%preun']
            SPEC_FILE += config['pkg']['prerm']
            SPEC_FILE += ['']

            SPEC_FILE += ['%files']
            SPEC_FILE += ['%defattr(-,root,root)']
            SPEC_FILE += ['/' + config['pkg']['usr_src_dir']]
            SPEC_FILE += ['/' + config['pkg']['usr_include_dir']]
            SPEC_FILE += ['/' + config['pkg']['pkg_cfg_pc']]
            SPEC_FILE += ['']
            SPEC_FILE += ['%changelog' ]
            SPEC_FILE += ['* ' + build_date + ' ' + config['vendor']['full'] + ' ' + config['vendor']['email'] + '> - ' + config['pkg']['version'] + '-' + config['pkg']['release']]
            SPEC_FILE += ['- ' + config['pkg']['changelog']]

            spec_file_name = os.path.abspath(os.path.join(out_dir, 'SPECS', 'specfile.spec'))
            log_info('GPKG-25', 'Writing spec file: ' + spec_file_name)
            with open(spec_file_name, mode='w') as outfile:
                outfile.write('\n'.join(SPEC_FILE))
            check_file_exists('GPKG-5', spec_file_name)

        else:
            config['pkg']['descr'] = '\n '.join(config['pkg']['descr']); # DEB package format requires \n+SPACE
            deb_name = config['pkg']['name'] + '_' + config['pkg']['version'] + '-' + config['pkg']['release'] + '_' + config['system']['arch']
            dest_base  = abspath(join(out_dir, deb_name))
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

            # Create postinst file
            POSTINST = config['pkg']['postinst']
            postinst_file_name = abspath(join(debian_dir, 'postinst'))
            log_info('GPKG-28', 'Writing postinst file:  ' + postinst_file_name)
            with open(postinst_file_name, mode='w') as outfile:
                outfile.write('\n'.join(POSTINST))
            os.chmod(postinst_file_name, 509); # octal 775
            check_file_exists('GPKG-05', postinst_file_name)

            # Create prerm file
            PRERM = config['pkg']['prerm']
            prerm_file_name = abspath(join(debian_dir, 'prerm'))
            log_info('GPKG-28', 'Writing prerm file:     ' + prerm_file_name)
            with open(prerm_file_name, mode='w') as outfile:
                outfile.write('\n'.join(PRERM))
            os.chmod(prerm_file_name, 509); # octal 775
            check_file_exists('GPKG-05', prerm_file_name)

            # Create preinst file
            PREINST = config['pkg']['preinst']
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
            CHANGE_LOG += ['-- ' + config['vendor']['full']+' <' + config['vendor']['email'] + '> ' + build_date + ' 00:00:00 +0000']
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

        driver_dest = [
            {
                'src': abspath(join(PROJECT_DIR, f)),
                'dst': join(config['pkg']['usr_src_dir'], os.path.dirname(f))
            } for f in driver
        ]

        # Copying packaged files
        SRC_DEST_LIST = [
            {'src': abspath(join(PROJECT_DIR, 'driver',   'Makefile' )),'dst': join(config['pkg']['usr_src_dir'], 'driver')},
            {'src': abspath(join(SCRIPT_DIR,  'pkg_data', 'dkms.conf')),'dst': config['pkg']['usr_src_dir']},
            *driver_dest
        ]

        for src_dest in SRC_DEST_LIST:
            src = src_dest['src']
            dst = dest_base
            if src_dest['dst'] != '':
                dst = abspath(join(dst, src_dest['dst']))
            copy_source_file('GPKG-31', src, dst)
            check_file_exists('GPKG-05', abspath(join(dst, os.path.basename(src))))
            if src.endswith('Makefile'):
                append_git_to_makefile('GPKG-05', abspath(join(dst, os.path.basename(src))))

            # Replace version in dkms.conf
            if src.endswith('dkms.conf'):
                with open(src, 'r') as dkms_src:
                    conf = dkms_src.read()
                    with open(join(dst, os.path.basename(src)), 'w') as dkms_dest:
                        new_conf = conf.replace('@PKGVER@', config['pkg']['version'])
                        dkms_dest.write(new_conf)

        # Create module PC file
        MODULE_PC = [
            'includedir=/'  + config['pkg']['usr_include_dir'],
            '',
            'Name: '        + config['pkg']['name'],
            'Description: ' + config['pkg']['summary'],
            'Version: '     + config['pkg']['version'],
            'Cflags: -I${includedir}'
        ]
        os.makedirs(abspath(join(dest_base, config['pkg']['pkg_cfg_dir'])))
        mod_pc_file_name = abspath(join(dest_base, config['pkg']['pkg_cfg_pc']))
        log_info('GPKG-28', 'Writing module PC file: ' + mod_pc_file_name)
        with open(mod_pc_file_name, mode='w') as outfile:
            outfile.write('\n'.join(MODULE_PC))
        check_file_exists('GPKG-05', mod_pc_file_name)

        # Generate package
        if config['system']['dist_id'].lower() in [d.lower() for d in DIST_RPM]:
            cmd = ['rpmbuild', '--verbose', '--define', '_topdir ' + out_dir, '-bb', spec_file_name]
            log_file_name = os.path.abspath(os.path.join(log_dir, 'rpmbuild.log'))

            pkg_name  = config['pkg']['name'] + '-' + config['pkg']['version'] + '-' + config['pkg']['release'] + '.' + config['system']['arch']
            pkg  = os.path.abspath(os.path.join(out_dir, 'RPMS', config['system']['arch'], pkg_name + '.rpm'))
            pkg_cpy  = os.path.abspath(os.path.join(out_dir, pkg_name + '.rpm'))
        else:
            cmd = ['dpkg-deb', '--build', '--root-owner-group', join(out_dir, deb_name)]
            log_file_name = abspath(join(log_dir, 'dpkg-deb.log'))

            pkg_name = config['pkg']['name'] + '_' + config['pkg']['version'] + '-' + config['pkg']['release'] + '_' + config['system']['arch']
            pkg     = abspath(join(out_dir, pkg_name + '.deb'))
            pkg_cpy = abspath(join(out_dir, pkg_name + '.deb'))

        log_newline()
        log_info('GPKG-37', 'Building package...')
        exec_step_cmd('GPKG-37', step, cmd, log_file_name)
        check_file_exists('GPKG-05', pkg)
        shutil.move(pkg, pkg_cpy)
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
