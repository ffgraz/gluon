#!/usr/bin/env python3
# requires: gluon-firewall|gluon-ebtables gluon-firewall-filter-multicast|gluon-ebtables-filter-multicast gluon-firewall-filter-ra-dhcp|gluon-ebtables-filter-ra-dhcp gluon-firewall-limit-arp|gluon-ebtables-limit-arp
"""Sends every kind of packet the client-bridge firewall is meant to
allow or block from a client, and checks on that same node what the
firewall did with it.

Both observations are taken on the sending node. A second client is not
a usable vantage point: Gluon filters most multicast towards the mesh,
and batman-adv then makes multicast decisions of its own, so nothing
about the far end would say what the firewall decided.

  * the mesh-facing device's transmit counter decides the verdict. It
    moves for exactly the frames the bridge firewall let through, and
    it is quiet otherwise, which no other counter on the node is.
  * the firewall's drop counters corroborate it where the backend can
    report them - gluon's nftables drop rules carry an explicit
    counter, while ebtables-tiny has none.

Asserting on effect rather than on rule syntax keeps the expectations
identical for either backend, which is what makes this a
reproducibility test for the ebtables->nftables migration.

Needs root (client taps) and scapy on the host.
"""
from pynet import start, finish
from meshlib import (pair, wait_connected, attach_client, Client,
                     send_from, drop_counters, mesh_dev, mesh_tx,
                     flood_multicast)

# Sent COPIES times, so a verdict never rests on a single frame.
COPIES = 20

CASES = {
    # A client must not be able to play router or DHCP server: these
    # have to be dropped, and must not reach the mesh.
    'rogue_router_advertisement': (
        'sendp(Ether(dst="33:33:00:00:00:01")/IPv6(dst="ff02::1")/'
        'ICMPv6ND_RA()/ICMPv6NDOptPrefixInfo(prefix="2001:db8:dead::", prefixlen=64),'
        ' iface=IFACE, count={n}, verbose=0)', False),
    'rogue_dhcpv6_reply': (
        'sendp(Ether(dst="33:33:00:01:00:02")/IPv6(dst="ff02::1:2")/'
        'UDP(sport=547, dport=546)/Raw(b"\\x07rogue"),'
        ' iface=IFACE, count={n}, verbose=0)', False),
    'rogue_dhcpv4_offer': (
        'sendp(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0", dst="255.255.255.255")/'
        'UDP(sport=67, dport=68)/Raw(b"rogue"), iface=IFACE, count={n}, verbose=0)',
        False),
    # The all-nodes group is not carried across the mesh.
    'all_nodes_multicast': (
        'sendp(Ether(dst="33:33:00:00:00:01")/IPv6(dst="ff02::1")/'
        'ICMPv6EchoRequest(id=0x4712), iface=IFACE, count={n}, verbose=0)', False),

    # A client must still be able to ask for a router or an address,
    # and to reach respondd's mesh-wide group.
    'router_solicitation': (
        'sendp(Ether(dst="33:33:00:00:00:02")/IPv6(dst="ff02::2")/ICMPv6ND_RS(),'
        ' iface=IFACE, count={n}, verbose=0)', True),
    'dhcpv6_solicit': (
        'sendp(Ether(dst="33:33:00:01:00:02")/IPv6(dst="ff02::1:2")/'
        'UDP(sport=546, dport=547)/Raw(b"\\x01solicit"),'
        ' iface=IFACE, count={n}, verbose=0)', True),
    'respondd_multicast': (
        'sendp(Ether(dst="33:33:00:02:10:01")/IPv6(dst="ff05::2:1001")/'
        'UDP(sport=32000, dport=1001)/Raw(b"GET nodeinfo"),'
        ' iface=IFACE, count={n}, verbose=0)', True),
}

a, b = pair()
attach_client(a)

start()

wait_connected(a, b)
flood_multicast(a)

client = Client(a)
client.move_to(a)
client.wait_addr()

dev = mesh_dev(a)
a.dbg('watching mesh-facing device ' + dev)

results = {}
for name, (expr, should_pass) in sorted(CASES.items()):
    drops_before, tx_before = drop_counters(a), mesh_tx(a, dev)
    send_from(client, expr.format(n=COPIES))
    a.execute('sleep 2')
    forwarded = mesh_tx(a, dev) - tx_before
    drops_now = drop_counters(a)
    dropped = None if drops_now is None else drops_now - drops_before

    results[name] = (forwarded >= COPIES / 2, should_pass, dropped, forwarded)
    a.dbg('{:<28} mesh_tx={:<5} dropped={:<6} expected={}'.format(
        name, forwarded, 'n/a' if dropped is None else dropped,
        'pass' if should_pass else 'block'))

print('\n--- firewall packet matrix ---')
bad = []
for name, (passed, should_pass, dropped, forwarded) in sorted(results.items()):
    ok = passed == should_pass
    print('{:<28} {:<9} mesh_tx={:<5} dropped={:<6} expected={}'.format(
        name, 'OK' if ok else 'MISMATCH', forwarded,
        'n/a' if dropped is None else dropped,
        'pass' if should_pass else 'block'))
    if not ok:
        bad.append(name)

if bad:
    raise AssertionError('firewall behaviour differs for: ' + ', '.join(bad))

finish()
