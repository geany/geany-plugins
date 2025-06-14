#!/usr/bin/env python3

import os
import shutil
import sys
import glob
from subprocess import check_call
from os.path import exists, isfile, join
import re

"""
This script prepares a Geany release on Windows.
The following steps will be executed:
- strip binary files (geany.exe, plugin .dlls)
- create installers
"""

if 'GITHUB_WORKSPACE' in os.environ:
    SOURCE_DIR = os.environ['GITHUB_WORKSPACE']
    BASE_DIR = join(SOURCE_DIR, 'geany_build')
    #TODO PARSE VERSION FROM configure.ac
else:
    # adjust paths to your needs ($HOME is used because expanduser() returns the Windows home directory)
    BASE_DIR = join(os.environ['HOME'], 'geany_build')
    SOURCE_DIR = join(os.environ['HOME'], 'git', 'geany-plugins')
VERSION="2.0"
with open(join(SOURCE_DIR, 'configure.ac'), 'r') as f:
    for line in f:
        # Look for lines containing 'AC_INIT'
        if 'AC_INIT' in line:
            # Use a regular expression to find the pattern:
            # AC_INIT([ProjectName], [Version], ...)
            # We want to capture the content of the second square bracket group.
            # The regex looks for 'AC_INIT(', then '[anything]', then a comma
            # and optional spaces, then captures '[Version]' into group 1.
            match = re.search(r'AC_INIT\(\[[^\]]+],\s*\[([^\]]+)\].*', line)
            if match:
                VERSION = match.group(1)
                print(f"GOT VERSION {VERSION} FROM {line}")
            else:
                print(f"!! FAILED TO GET VERSION FROM {line}")
            break # Stop after finding the first AC_INIT line with a version
RELEASE_DIR_ORIG = join(BASE_DIR, 'release', 'geany-plugins-orig')
RELEASE_DIR = join(BASE_DIR, 'release', 'geany-plugins')
BUNDLE_BASE_DIR = join(BASE_DIR, 'bundle')
BUNDLE_GEANY_PLUGINS = join(BASE_DIR, 'bundle', 'geany-plugins-dependencies')
INSTALLER_NAME = join(BASE_DIR, f'geany-plugins-{VERSION}_setup.exe')


def run_command(*cmd):
    print('Execute command: {}'.format(' '.join(cmd)))
    check_call(cmd)


def prepare_release_dir():
    os.makedirs(RELEASE_DIR_ORIG, exist_ok=True)
    if exists(RELEASE_DIR):
        shutil.rmtree(RELEASE_DIR)
    shutil.copytree(RELEASE_DIR_ORIG, RELEASE_DIR, symlinks=True, ignore=None)


def convert_text_files(*paths):
    for item in paths:
        files = glob.glob(item)
        for filename in files:
            if isfile(filename):
                run_command('unix2dos', '--quiet', filename)


def strip_files(*paths):
    for item in paths:
        files = glob.glob(item)
        for filename in files:
            run_command('strip', filename)


def make_release():
    # copy the release dir as it gets modified implicitly by signing and converting files, we want to keep a pristine version before we start
    prepare_release_dir()

    binary_files = (
        f'{RELEASE_DIR}/bin/libgeanypluginutils-0.dll',
        f'{RELEASE_DIR}/lib/*.dll',
        f'{RELEASE_DIR}/lib/geany/*.dll',
        f'{RELEASE_DIR}/lib/geany-plugins/geanylua/libgeanylua.dll')
    # strip binaries
    strip_files(*binary_files)
    # unix2dos conversion
    text_files = (
        f'{RELEASE_DIR}/share/doc/geany-plugins/*/AUTHORS',
        f'{RELEASE_DIR}/share/doc/geany-plugins/*/COPYING',
        f'{RELEASE_DIR}/share/doc/geany-plugins/*/ChangeLog',
        f'{RELEASE_DIR}/share/doc/geany-plugins/*/NEWS',
        f'{RELEASE_DIR}/share/doc/geany-plugins/*/README',
        f'{RELEASE_DIR}/share/doc/geany-plugins/*/manual.rst')
    convert_text_files(*text_files)
    # create installer
    run_command(
        'makensis',
        '/WX',
        '/V3',
        f'/DGEANY_PLUGINS_RELEASE_DIR={RELEASE_DIR}',
        f'/DDEPENDENCY_BUNDLE_DIR={BUNDLE_GEANY_PLUGINS}',
        f'-DGEANY_PLUGINS_INSTALLER_NAME={INSTALLER_NAME}',
        f'{SOURCE_DIR}/build/geany-plugins.nsi')


if __name__ == '__main__':
    make_release()
