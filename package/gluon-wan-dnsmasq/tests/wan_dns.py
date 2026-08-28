#!/usr/bin/env python3
# requires: gluon-mesh-vpn-core
"""gluon-wan-dnsmasq gives the node a resolver that goes out over the
uplink rather than through the client-facing dnsmasq.

It is deliberately not the node's general resolver: a plain lookup on
the node still lands on the client dnsmasq, which has no upstream and
refuses. Only the gluon-mesh-vpn group's queries are redirected to it,
which is what lets the VPN daemon resolve its peers while a resolver
loop stays impossible. Both halves of that are asserted here, so the
redirect cannot quietly stop applying.
"""
from pynet import Node, start, finish
from meshlib import DNS_NAME, wait_uplink, as_mesh_vpn

a = Node()

start()

wait_uplink(a, 4)

# The uplink-side resolver itself answers.
a.wait_until_succeeds('nslookup {} 127.0.0.1:54 >/dev/null'.format(DNS_NAME))

# ... and the mesh-vpn group reaches it without asking for that port.
status, out = a.execute(as_mesh_vpn('nslookup ' + DNS_NAME))
if status != 0:
    raise AssertionError(
        'mesh-vpn DNS is not redirected to gluon-wan-dnsmasq:\n' + out)

finish()
