#!/usr/bin/env python3
"""With gluon-wan-dnsmasq the node itself can resolve names over its
uplink. Without the package a node has no recursive resolver of its own
- its dnsmasq only serves clients - so this belongs here and not with
the generic uplink test."""
from pynet import Node, start, finish
from meshlib import DNS_NAME, wait_uplink, resolves_dns

a = Node()

start()

wait_uplink(a, 4)
a.wait_until_succeeds('nslookup {} >/dev/null'.format(DNS_NAME))

if not resolves_dns(a):
    raise AssertionError('cannot resolve ' + DNS_NAME)

finish()
