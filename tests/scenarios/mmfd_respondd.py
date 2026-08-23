#!/usr/bin/env python3
# protocols: babel olsrd
"""mmfd forwards respondd multicast across the whole layer-3 mesh:
a query on one end of a chain gets answers from all nodes."""
from pynet import start, finish
from meshlib import chain, wait_connected

a, b, c = chain(3)

start()

wait_connected(a, c)

# respondd listens on the device from client.dev (the mmfd interface);
# replies from b and c can only arrive if mmfd forwards the multicast
a.wait_until_succeeds(
    '[ "$(gluon-neighbour-info -d ff02::2:1001 -p 1001 -r nodeinfo'
    ' -i "$(cat /lib/gluon/respondd/client.dev)" -c 5 | wc -l)" -ge 3 ]')

finish()
