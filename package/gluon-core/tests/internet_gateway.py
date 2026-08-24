#!/usr/bin/env python3
"""A node with an uplink reaches the internet over IPv4 and IPv6.

The IPv6 reachability check only applies when the uplink actually has a
global address: QEMU's user networking hands out the deprecated
fec0::/64, which Linux will not source global traffic from, so in that
lab only the uplink itself can be checked. Targets are overridable via
GLUON_TEST_V4_TARGET and GLUON_TEST_V6_TARGET.
"""
from pynet import Node, start, finish
from meshlib import V4_TARGET, V6_TARGET, wait_uplink, reaches_internet

a = Node()

start()

failures = []

wait_uplink(a, 4)
if not reaches_internet(a, 4, V4_TARGET):
    failures.append('no IPv4 connectivity to ' + V4_TARGET)

wait_uplink(a, 6)
status, _ = a.execute('ip -6 addr show dev br-wan | grep -qE "inet6 [23]"')
if status == 0:
    if not reaches_internet(a, 6, V6_TARGET):
        failures.append('no IPv6 connectivity to ' + V6_TARGET)
else:
    a.dbg('uplink has no global IPv6 address, only checking the uplink '
          'itself is up (QEMU user networking provides fec0::/64)')

if failures:
    raise AssertionError('; '.join(failures))

finish()
