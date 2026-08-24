#!/usr/bin/env python3
# requires: gluon-firewall|gluon-ebtables gluon-firewall-filter-multicast|gluon-ebtables-filter-multicast gluon-firewall-filter-ra-dhcp|gluon-ebtables-filter-ra-dhcp gluon-firewall-limit-arp|gluon-ebtables-limit-arp
"""Sends one packet of every kind the client-bridge firewall is meant to
allow or block from a client, and checks on that same node what the
firewall did with it.

Both observations are taken on the sending node, because Gluon filters
most multicast towards the mesh, so a second client is not a reliable
place to look:

  * the firewall's drop counters, which say whether a rule caught the
    packet - ebtables counts every rule, and gluon's nftables drop rules
    carry an explicit counter
  * the mesh interfaces' transmit counters, which say whether the frame
    actually left towards the mesh

Asserting on effect rather than on rule syntax keeps the expectations
identical for either backend, which is what makes this a reproducibility
test for the ebtables->nftables migration.

Needs root (client taps) and scapy on the host.
"""
from pynet import start, finish
from meshlib import (pair, wait_connected, attach_client, Client,
                     send_from, drop_counters, mesh_tx)

# Sent COPIES times each, so the counter deltas stand clear of the
# background chatter of a running mesh.
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

    # A client must still be able to ask for a router or an address.
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

client = Client(a)
client.move_to(a)
client.wait_addr()

results = {}
for name, (expr, should_pass) in sorted(CASES.items()):
    drops_before, tx_before = drop_counters(a), mesh_tx(a)
    send_from(client, expr.format(n=COPIES))
    a.execute('sleep 2')
    dropped = drop_counters(a) - drops_before
    forwarded = mesh_tx(a) - tx_before

    # A blocked kind shows up in the drop counters; an allowed one
    # reaches the mesh. Both are counted against COPIES so that the
    # mesh's own background traffic cannot decide the outcome.
    blocked = dropped >= COPIES / 2
    passed = forwarded >= COPIES / 2
    results[name] = (blocked, passed, should_pass, dropped, forwarded)
    a.dbg('{:<28} dropped={:<4} mesh_tx={:<4} expected={}'.format(
        name, dropped, forwarded, 'pass' if should_pass else 'block'))

print('\n--- firewall packet matrix ---')
bad = []
for name, (blocked, passed, should_pass, dropped, forwarded) in sorted(results.items()):
    ok = passed if should_pass else blocked
    print('{:<28} {:<9} dropped={:<5} mesh_tx={:<5} expected={}'.format(
        name, 'OK' if ok else 'MISMATCH', dropped, forwarded,
        'pass' if should_pass else 'block'))
    if not ok:
        bad.append(name)

if bad:
    raise AssertionError('firewall behaviour differs for: ' + ', '.join(bad))

finish()
