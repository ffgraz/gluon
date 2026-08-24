#!/usr/bin/env python3
"""A node with an uplink reaches the internet over IPv4 and IPv6, and
resolves DNS. Targets are overridable via GLUON_TEST_V4_TARGET,
GLUON_TEST_V6_TARGET and GLUON_TEST_DNS_NAME for disconnected labs."""
from pynet import Node, start, finish
from meshlib import (V4_TARGET, V6_TARGET, DNS_NAME,
                     wait_uplink, reaches_internet, resolves_dns)

a = Node()

start()

failures = []

for family, target in ((4, V4_TARGET), (6, V6_TARGET)):
    wait_uplink(a, family)
    if not reaches_internet(a, family, target):
        failures.append('no IPv{} connectivity to {}'.format(family, target))

if not resolves_dns(a):
    failures.append('cannot resolve ' + DNS_NAME)

if failures:
    raise AssertionError('; '.join(failures))

finish()
