#!/usr/bin/env python
# -*- coding: utf-8 -*-
#.--. .-. ... .... -. - ... .-.-.- .. -.

import sys
import os

from setuptools import setup, Extension

def check_rna_lib_installed(**kwa):
  "Check Vienna RNA Packages"
  return all([os.path.exists(*v) for k, v in kwa.items()])

def get_rna_lib_intallation_path():
  "Attempts to get the RNAlib Path"
  return {
    'include_dirs': [
      os.environ.get('VIENNARNAINCLUDE', '/usr/local/include/ViennaRNA'),
    ],
    'library_dirs': [
      os.environ.get('VIENNARNALIB', '/usr/local/lib')
    ]
  }

def check_sanity():
  """
  Check Sanity of the System. Since miRador core depends on C extension,
  only posix systems are supported at the moment.
  """

  if not os.name == 'posix':
    print("miRador is currently not tested on non-posix systems.")

  if sys.version_info[:2] not in [(2, 7), (3, 3), (3, 4), (3, 5)]:
    print("miRador is only supported on Python 2.7, 3.3, 3.4, 3.5.")
    return False

  if not check_rna_lib_installed(**get_rna_lib_intallation_path()):
    print(
      "miRador requires Vienna RNA package. Please read the docs "
      "for installation instructions.\n"
      "If you have Vienna Package, then perhaps the installer is "
      "unable to find the location. The location may be set by "
      "setting the `VIENNARNAINCLUDE` and `VIENNARNALIB` environment "
      "variables."
    )
    return False
  return True

if not check_sanity():
  sys.exit(1)

def extension_args():
  args = get_rna_lib_intallation_path()
  args['sources'] = [
    'mirador/pymiranda/pymiranda.c',
    'mirador/pymiranda/scan.c',
    'mirador/pymiranda/swat.c',
    'mirador/pymiranda/thermo.c',
    'mirador/pymiranda/utils.c',
    'mirador/pymiranda/output.c',
    'mirador/pymiranda/ExpString.c',
  ]
  args['libraries'] = [
    'RNA',
    'm'
  ]
  return args

setup_args = {
  'name': 'mirador',
  'version': '4.0',
  'description': 'This is a demo package',
  'author': 'Prashant Sinha',
  'author_email': 'prashant@ducic.ac.in',
  'url': 'https://github.com/prashnts/mirador',
  'license': 'GPLv3',
  'description': 'Finds target sites for the miRNAs in genomic sequences.',
  'packages': ['mirador'],
  'ext_modules': [
    Extension('mirador.pymiranda', **extension_args()),
  ],
  'classifiers': [
    'Development Status :: 4 - Beta',
    'Intended Audience :: Developers',
    'Intended Audience :: Science/Research',
    'License :: OSI Approved :: GNU General Public License v3 (GPLv3)',
    'Programming Language :: Python',
    'Programming Language :: Python :: 2',
    'Programming Language :: Python :: 3',
    'Topic :: Scientific/Engineering :: Bio-Informatics',
  ]
}

setup(**setup_args)