#!/usr/bin/env python3
"""mmfd carries multicast across the whole layer-3 mesh: a packet sent
to the mesh-wide respondd group on one end of a chain is delivered on
the far end, two hops away.

Delivery is asserted on the receiving nodes rather than by collecting
replies: every node carries the same fe80::1 on mmfd0, so a node that
queries the group would have the answers addressed back to its own
interface. The mesh-wide group is meant to be queried by a monitoring
host with a routable address, not node to node.
"""
from pynet import start, finish
from meshlib import (chain, wait_connected, proto, respondd_dev,
                     respondd_query, CONVERGENCE_TIMEOUT)

a, b, c = chain(3)

start()

wait_connected(a, c)

RX = 'cat /sys/class/net/mmfd0/statistics/rx_packets'
before = {node.hostname: int(node.succeed(RX)) for node in (b, c)}
a.dbg('mmfd0 rx before: {}'.format(before))

# Sent from one end of the chain; mmfd has to carry it to the direct
# neighbour and on to the node two hops away.
query = respondd_query(respondd_dev(a), count=1, timeout=2)
for _ in range(3):
    a.execute(query)

for node in (b, c):
    node.wait_until_succeeds(
        '[ "$({})" -gt {} ]'.format(RX, before[node.hostname]),
        CONVERGENCE_TIMEOUT[proto(a)])
    node.dbg('received forwarded multicast')

finish()
