#!/usr/bin/env python3
"""Boot one node and print the packages it has installed, one per line,
then its site config. run.py caches both next to the image: the packages
decide which tests apply, the site config parametrises gateway images."""
from pynet import Node, start, finish

a = Node()

start()

# Both before the markers: succeed() logs to stdout as well.
packages = a.succeed(
    'apk info 2>/dev/null || opkg list-installed 2>/dev/null | cut -d" " -f1')
site = a.succeed('gluon-show-site')

print('--- packages ---')
for line in sorted(set(packages.split())):
    print(line)

print('--- site ---')
print(site)

finish()
