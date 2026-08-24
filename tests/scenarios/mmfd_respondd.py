#!/usr/bin/env python3
# protocols: babel olsrd
"""mmfd forwards respondd multicast across the whole layer-3 mesh: a
query from one end of a chain is answered by every node, including the
one two hops away."""
from pynet import start, finish
from meshlib import (chain, wait_connected, proto, respondd_dev,
                     respondd_query, CONVERGENCE_TIMEOUT)

a, b, c = chain(3)

start()

wait_connected(a, c)

# Answers from b and c can only arrive if mmfd carries the site-local
# respondd group across the mesh.
dev = respondd_dev(a)
a.dbg('querying respondd via ' + dev)
a.wait_until_succeeds(
    '[ "$({} | wc -l)" -ge 3 ]'.format(respondd_query(dev, count=5)),
    CONVERGENCE_TIMEOUT[proto(a)])

finish()
