#!/usr/bin/env python3
"""batman-adv carries clients in its translation table: a node claims
the clients attached to it, announces them to the mesh under its own
originator address, and hands the claim on when one roams away.

The topology is a chain, so the far end is two hops from the client.
That is where the translation table is the only thing that can make it
reachable - a neighbour could have learned the client from seeing its
frames, a node two hops away could not - and the middle node is a
vantage point that hosts the client neither before nor after the roam.

Requires root, for the client tap and its network namespace.
"""
from pynet import start, finish
from meshlib import (
    chain, wait_connected, attach_client, Client, CONVERGENCE_TIMEOUT)

TIMEOUT = CONVERGENCE_TIMEOUT['batman-adv']

a, b, c = chain(3)
attach_client(a)
attach_client(c)

start()

wait_connected(a, c)
wait_connected(c, a)


def originator(node):
    """The address the node announces its clients under."""
    return node.succeed('cat /sys/class/net/primary0/address').strip()


def wait_claimed(node, mac):
    """Wait until the node claims the client as its own."""
    node.wait_until_succeeds(
        "batctl tl | grep -qi -- '{}'".format(mac), TIMEOUT)


def wait_announced(node, mac, via):
    """Wait until the node has learned the client sits behind via."""
    node.wait_until_succeeds(
        "batctl tg | grep -i -- '{}' | grep -qi -- '{}'".format(mac, via),
        TIMEOUT)


def wait_withdrawn(node, mac, via):
    """Wait until the node has stopped believing the client sits behind
    via, so a roam replaces the announcement rather than adding one."""
    node.wait_until_succeeds(
        "! batctl tg | grep -i -- '{}' | grep -qi -- '{}'".format(mac, via),
        TIMEOUT)


orig_a, orig_c = originator(a), originator(c)
b.dbg('originators: node1={} node3={}'.format(orig_a, orig_c))

client = Client(a, c)

# The client appears behind node a and is reached from the far end of
# the chain. That exchange is also what teaches the mesh where it is: a
# client that has only just configured itself has sent nothing but
# multicast, and most client multicast is filtered before it reaches
# the mesh, so nothing has had reason to claim it yet.
client.move_to(a)
addr = client.wait_addr()
b.dbg('client {} is {}'.format(client.mac, addr))
c.wait_until_succeeds('ping -c 3 {}'.format(addr), TIMEOUT)

# Which is recorded where it should be: node a claims the client, and
# the rest of the mesh has learned that it sits behind a.
wait_claimed(a, client.mac)
wait_announced(b, client.mac, orig_a)

# It roams to the other end of the chain, and stays reachable - now
# from the node it left.
client.move_to(c)
client.wait_addr()
a.wait_until_succeeds('ping -c 3 {}'.format(addr), TIMEOUT)

# The claim moved with it, and the announcement the mesh holds was
# replaced rather than added to. The middle node is asked because it
# hosts the client neither before nor after, so it has to hold an
# announcement in both cases - the two ends each answer from their own
# local table for half of this.
wait_claimed(c, client.mac)
wait_announced(b, client.mac, orig_c)
wait_withdrawn(b, client.mac, orig_a)

finish()
