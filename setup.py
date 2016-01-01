#!/bin/env python
# Copyright (c) 2012 Thales Global Services SAS
# 
# Author : Robin Jarry
# 
# The MIT license. See LICENSE file for details

import sys

if sys.version_info < (2, 5) or sys.version_info >= (3,):
    sys.stderr.write('ERROR: "sonar-rules-extractor" only works with Python versions >= 2.5 and < 3.0\n')
    sys.exit(1)

try:
    from setuptools import setup, find_packages
except ImportError:
    sys.stderr.write('ERROR: "sonar-rules-extractor" is missing a Python dependency: "setuptools". Please install it first.\n')
    sys.exit(1)

import sonar_rules_extractor

setup(    
    name = 'sonar-rules-extractor',
    version = sonar_rules_extractor.__version__,
    license = 'MIT',
    author = 'Robin Jarry',
    author_email = 'robin.jarry@external.thalesgroup.com',
    url = 'http://pypi.python.org/pypi/sonar-rules-extractor',
    description = 'Coding rules extractor into the Sonar format.',
    long_description = open('README').read(),
    platforms = 'any',
    classifiers = [
        'Development Status :: 4 - Beta',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Programming Language :: Python',
        'Programming Language :: Python :: 2',
        'Topic :: Documentation',
        'Topic :: Text Processing',
        'Topic :: Utilities',
    ],

    # DEPENDENCIES
    provides = ['sonar_rules_extractor'],

    # CONTENTS
    packages = find_packages(),
    include_package_data = True,
    zip_safe = False,
    entry_points = {
        'console_scripts': ('sonar-rules-extractor = sonar_rules_extractor.cmdline:run'),
    },
)

