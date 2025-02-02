#!/usr/bin/env python3

import os
import shutil
import sys
import glob
from subprocess import check_call
from os.path import exists, isfile, join

"""
This script prepares a Geany release on Windows.
The following steps will be executed:
- strip binary files (geany.exe, plugin .dlls)
- sign binary files with certificate
- create installers
- sign installers
"""

VERSION = '2.1'
# adjust paths to your needs ($HOME is used because expanduser() returns the Windows home directory)
BASE_DIR = join(os.environ['HOME'], 'geany_build')
SOURCE_DIR = join(os.environ['HOME'], 'git', 'geany-plugins')
RELEASE_DIR_ORIG = join(BASE_DIR, 'release', 'geany-plugins-orig')
RELEASE_DIR = join(BASE_DIR, 'release', 'geany-plugins')
BUNDLE_BASE_DIR = join(BASE_DIR, 'bundle')
BUNDLE_GEANY_PLUGINS = join(BASE_DIR, 'bundle', 'geany-plugins-dependencies')
INSTALLER_NAME = join(BASE_DIR, f'geany-plugins-{VERSION}_setup.exe')

# signing params
SIGN_CERTIFICATE = join(BASE_DIR, 'codesign.pem')  # adjust to your needs
SIGN_CERTIFICATE_KEY = join(BASE_DIR, 'codesign_key.pem')  # adjust to your needs
SIGN_WEBSITE = 'https://www.geany.org'
SIGN_NAME = 'Geany-Plugins Binary'
SIGN_TIMESTAMP = 'https://zeitstempel.dfn.de/'


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


def sign_files(*paths):
    if not isfile(SIGN_CERTIFICATE_KEY):
        print('Skipped signing {} as {} not found'.format(paths, SIGN_CERTIFICATE_KEY))
        return
    for item in paths:
        files = glob.glob(item)
        for filename in files:
            run_command(
                'osslsigncode',
                'sign',
                '-verbose',
                '-certs', SIGN_CERTIFICATE,
                '-key', SIGN_CERTIFICATE_KEY,
                '-n', SIGN_NAME,
                '-i', SIGN_WEBSITE,
                '-ts', SIGN_TIMESTAMP,
                '-h', 'sha512',
                '-in', filename,
                '-out', f'{filename}-signed')
            os.replace(f'{filename}-signed', filename)


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
    # sign binaries
    sign_files(*binary_files)
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
    # sign installer
    sign_files(f'{INSTALLER_NAME}')


if __name__ == '__main__':
    make_release()
