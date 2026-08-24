#!/usr/bin/env python3
# requires: gluon-firewall-filter-multicast gluon-firewall-filter-ra-dhcp gluon-firewall-limit-arp
"""Sends one packet of every kind the client-bridge firewall is meant to
allow or block from a client behind one node, and checks at a client
behind the other node whether it arrived.

This is the reproducibility test for the ebtables->nftables migration:
it asserts what actually crosses the bridge, not how the rules are
written, so the same expectations must hold for either backend. The
whole observed matrix is printed, so a first run against a new backend
shows every difference at once instead of stopping at the first.

Needs root (client taps), scapy and tcpdump on the host.
"""
import functools

from pynet import start, finish
from meshlib import (pair, wait_connected, attach_client, Client,
                     send_from, capture_while)

# name -> (scapy expression sent from the first client,
#          capture filter on the second client,
#          must the packet arrive?)
CASES = {
    # Ordinary client traffic must pass.
    'icmpv6_echo_unicast': (
        'sendp(Ether(dst=PEER_MAC)/IPv6(dst=PEER_IP6)/ICMPv6EchoRequest(id=0x4711),'
        ' iface=IFACE, count=3, verbose=0)',
        'icmp6', True),

    # A client must not be able to play router.
    'rogue_router_advertisement': (
        'sendp(Ether(dst="33:33:00:00:00:01")/IPv6(dst="ff02::1")/'
        'ICMPv6ND_RA()/ICMPv6NDOptPrefixInfo(prefix="2001:db8:dead::", prefixlen=64),'
        ' iface=IFACE, count=3, verbose=0)',
        'icmp6', False),

    # A client must not be able to play DHCP server.
    'rogue_dhcpv6_reply': (
        'sendp(Ether(dst="33:33:00:01:00:02")/IPv6(dst="ff02::1:2")/'
        'UDP(sport=547, dport=546)/Raw(b"\\x07rogue"), iface=IFACE, count=3, verbose=0)',
        'udp port 546 or udp port 547', False),
    'rogue_dhcpv4_offer': (
        'sendp(Ether(dst="ff:ff:ff:ff:ff:ff")/IP(src="0.0.0.0", dst="255.255.255.255")/'
        'UDP(sport=67, dport=68)/Raw(b"rogue"), iface=IFACE, count=3, verbose=0)',
        'udp port 67 or udp port 68', False),

    # A client must still be able to ask for a router / an address.
    'router_solicitation': (
        'sendp(Ether(dst="33:33:00:00:00:02")/IPv6(dst="ff02::2")/ICMPv6ND_RS(),'
        ' iface=IFACE, count=3, verbose=0)',
        'icmp6', True),
    'dhcpv6_solicit': (
        'sendp(Ether(dst="33:33:00:01:00:02")/IPv6(dst="ff02::1:2")/'
        'UDP(sport=546, dport=547)/Raw(b"\\x01solicit"), iface=IFACE, count=3, verbose=0)',
        'udp port 546 or udp port 547', True),

    # Multicast policy: the all-nodes group and gluon's VXLAN group are
    # not carried across the mesh, respondd's group is.
    'all_nodes_multicast': (
        'sendp(Ether(dst="33:33:00:00:00:01")/IPv6(dst="ff02::1")/'
        'ICMPv6EchoRequest(id=0x4712), iface=IFACE, count=3, verbose=0)',
        'icmp6', False),
    'respondd_multicast': (
        'sendp(Ether(dst="33:33:00:02:10:01")/IPv6(dst="ff05::2:1001")/'
        'UDP(sport=32000, dport=1001)/Raw(b"GET nodeinfo"), iface=IFACE, count=3, verbose=0)',
        'udp port 1001', True),
}

a, b = pair()
attach_client(a)
attach_client(b)

start()

wait_connected(a, b)

sender = Client(a)
sender.move_to(a)
sender_ip6 = sender.wait_addr()
sender_mac = sender.mac

receiver = Client(b)
receiver.move_to(b)
receiver_ip6 = receiver.wait_addr()

# let both clients learn each other so unicast is not flooded-only
sender.succeed('ping -6 -c 3 -W 3 {} || true'.format(receiver_ip6))

results = {}
for name, (expr, pcap_filter, expected) in sorted(CASES.items()):
    expr = expr.replace('PEER_IP6', repr(receiver_ip6)).replace(
        'PEER_MAC', repr(receiver.mac))
    count = capture_while(receiver, pcap_filter,
                          functools.partial(send_from, sender, expr))
    arrived = count > 0
    results[name] = (arrived, expected, count)
    a.dbg('{:<28} arrived={!s:<5} expected={!s:<5} packets={}'.format(
        name, arrived, expected, count))

print('\n--- firewall packet matrix ---')
for name, (arrived, expected, count) in sorted(results.items()):
    print('{:<28} {:<9} arrived={!s:<5} expected={!s:<5} ({} pkt)'.format(
        name, 'OK' if arrived == expected else 'MISMATCH',
        arrived, expected, count))

bad = {n: v for n, v in results.items() if v[0] != v[1]}
if bad:
    raise AssertionError('firewall behaviour differs for: ' + ', '.join(sorted(bad)))

finish()
