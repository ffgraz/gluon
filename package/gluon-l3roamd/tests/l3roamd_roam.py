#!/usr/bin/env python3
"""A client roams from one end of a chain to the other; l3roamd keeps
its address reachable from across the mesh. Requires root."""
from pynet import start, finish
from meshlib import chain, wait_connected, attach_client, Client

a, b, c = chain(3)
attach_client(a)
attach_client(c)

start()

wait_connected(a, c)
wait_connected(c, a)

client = Client(a, c)

# client appears behind node a; the far end learns a route to it
client.move_to(a)
addr = client.wait_addr()
c.wait_until_succeeds('ping -c 3 ' + addr)

# client roams to node c; the mesh follows
client.move_to(c)
client.wait_addr()
a.wait_until_succeeds('ping -c 3 ' + addr)

finish()
