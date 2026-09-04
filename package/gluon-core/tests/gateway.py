#!/usr/bin/env python3
"""A gateway VM (tests/nix, declared in gateway.nix) joins the mesh: the
nodes learn their default route through it, and a client behind a node
reaches the world through its NAT.

Needs root (client taps) and nix on the host.
"""
from pynet import start, finish
from meshlib import (
    Gateway, pair, connect, attach_client, proto, wait_neighbours,
    wait_gateway, Client, V4_TARGET)

# Beyond the gateway lies QEMU's user network, which answers pings to its
# own router address but does not proxy ICMPv6 any further; so that is
# how far an IPv6 ping through the gateway's NAT66 can get.
UPLINK_ROUTER6 = 'fec0::2'

gw = Gateway('gateway')
a, b = pair()
connect(gw, a)
attach_client(b)

start()

p = proto(a)
wait_neighbours(a, 2)  # b and the gateway
wait_neighbours(b, 1)

# Layer-3 nodes learn the default route for IPv4 too when they have a
# node address of their own.
has4 = p != 'batman-adv' and a.execute(
    'ip -4 -o addr show dev lo | grep -qv 127.0.0.1')[0] == 0
for node in (a, b):
    wait_gateway(node, 6)
    if has4:
        wait_gateway(node, 4)

gw6 = gw.addr(6)
a.dbg('gateway address ' + gw6)
a.wait_until_succeeds('ping -c 3 -W 2 ' + gw6)
b.wait_until_succeeds('ping -c 3 -W 2 ' + gw6)

client = Client(b)
client.move_to(b)
client.wait_addr()
client.wait_until_succeeds('ping -c 3 -W 2 ' + gw6)
# Through the gateway's NAT66 to its uplink.
client.wait_until_succeeds('ping -c 3 -W 2 ' + UPLINK_ROUTER6)

if p == 'batman-adv':
    # The gateway is the mesh's DHCPv4 server as well.
    addr, router = client.dhcp4()
    a.dbg('client got {} via {}'.format(addr, router))
    client.wait_until_succeeds('ping -c 3 -W 2 ' + V4_TARGET)

finish()
