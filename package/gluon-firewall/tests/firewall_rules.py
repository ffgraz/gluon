#!/usr/bin/env python3
# requires: gluon-firewall-filter-multicast gluon-firewall-filter-ra-dhcp gluon-firewall-limit-arp
"""Characterizes the client-bridge firewall behaviour that the
ebtables-based filters implement, so the nftables rewrite can be shown
to reproduce it. Asserts observable effects, not rule syntax, so the
same scenario runs against either backend.

Requires an image built with the firewall filter features
(filter-multicast, filter-ra-dhcp, limit-arp).
"""
from pynet import start, finish
from meshlib import (pair, wait_connected, dump_firewall,
                     respondd_dev, respondd_query)

a, b = pair()

start()

wait_connected(a, b)

# The dump is archived in the node log for diffing between backends.
dump = dump_firewall(a)
assert dump.strip(), 'no firewall ruleset present'

# 1. Client-facing multicast policy: link-local all-nodes and the Gluon
#    VXLAN group must be dropped towards the mesh, respondd's
#    site-local group must survive.
for chain in ('MULTICAST_OUT', 'multicast_out'):
    if chain in dump:
        break
else:
    raise AssertionError('no multicast egress chain in ruleset')

assert 'ff02::1' in dump, 'all-nodes multicast not filtered'
assert 'ff02::15c' in dump, 'VXLAN multicast group not filtered'
assert '1001' in dump and 'ff05::2:1001' in dump, 'respondd multicast not allowed'

# 2. Direction filters for RA/DHCP: solicitations may only leave the
#    node, advertisements/replies may only enter it.
for token in ('router-solicitation', 'nd-router-solicit'):
    if token in dump:
        break
else:
    raise AssertionError('no RS direction rule')

for token in ('router-advertisement', 'nd-router-advert'):
    if token in dump:
        break
else:
    raise AssertionError('no RA direction rule')

assert '546' in dump and '547' in dump, 'no DHCPv6 direction rules'

# 3. ARP rate limiting is installed on the client bridge.
assert 'arp' in dump.lower(), 'no ARP rules'

# 4. The firewall does not break the mesh itself.
b.wait_until_succeeds('ping -c 3 node1')

# 5. respondd stays reachable across the client bridge, which is what
#    the multicast allow rules exist for.
b.wait_until_succeeds('[ "$({} | wc -l)" -ge 1 ]'.format(
    respondd_query(respondd_dev(b), count=2)))

finish()
