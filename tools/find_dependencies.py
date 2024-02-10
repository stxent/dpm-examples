#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# find_dependencies.py
# Copyright (C) 2024 xent
# Project is distributed under the terms of the GNU General Public License v3.0

'''Find dependencies in template files.

This module finds dependencies in recursive Jinja2 template files.
'''

import argparse
import os
import re

def find_dependencies(name):
    path = os.path.dirname(os.path.abspath(__file__))
    path_templates = os.path.abspath(os.path.join(path, '..', 'templates'))

    lines = []
    with open(os.path.join(path_templates, name), 'rb') as stream:
        lines = stream.read().decode().replace('\r', '').split('\n')

    included_files = []
    for line in lines:
        match = re.match(r'^.*?{%.*?extends.*?[\'\"]([^\'\"]+)[\'\"].*?%}.*$', line)
        if match is not None:
            included_files.append(match.group(1))

    recursive_files = []
    for included_name in included_files:
        recursive_files.extend(find_dependencies(included_name))

    return included_files + recursive_files

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(dest='templates', nargs='*')
    options = parser.parse_args()

    for path in options.templates:
        included_files = find_dependencies(path)
        print(';'.join([os.path.basename(path)] + included_files))

if __name__ == '__main__':
    main()
