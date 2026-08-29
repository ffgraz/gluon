#!/usr/bin/env python3
# requires: gluon-mesh-olsrd gluon-mesh-babel
"""On a hybrid image babel and olsrd run side by side: babel carries
IPv6, olsrd carries the older IPv4 network, and whether olsrd runs at
all is the node's own decision.

The point of the scenario is that clients do not notice either way, so
it takes a client through both settings. Requires root, for the client
taps.
"""
from pynet import start, finish
from meshlib import (
    chain, wait_connected, attach_client, proto, Client,
    CONVERGENCE_TIMEOUT)

def set_olsr(node, enabled):
    """Switch olsrd on or off the way the config mode page does."""
    node.succeed('uci set gluon.mesh_olsrd.enabled={}'.format(
        1 if enabled else 0))
    node.succeed('uci commit gluon')

    # Detaching it does not survive the ssh channel closing - the
    # reconfiguration gets killed halfway, with the services it stopped
    # left stopped. It keeps the connection, so run it and wait. What
    # it said is kept: when it goes wrong it goes wrong quietly, and
    # the scenario is then failing several steps further on.
    status, out = node.execute('gluon-reconfigure 2>&1')
    node.dbg(
        'gluon-reconfigure exited %d after %d lines'
        % (status, len(out.split('\n'))))
    if status != 0 or len(out.split('\n')) < 20:
        for line in out.split('\n')[-30:]:
            node.dbg('| ' + line)
        raise Exception('gluon-reconfigure did not run to completion')


def running(node, service):
    status, _ = node.execute('pgrep ' + service)
    return status == 0


def check_daemons(node, olsr):
    """babel always runs; olsrd only when switched on, and only for
    IPv4 - babel is carrying IPv6, so the second olsrd must stay down."""
    node.wait_until_succeeds('pgrep babeld')

    if olsr:
        node.wait_until_succeeds('pgrep olsrd')
    else:
        assert not running(node, 'olsrd'), \
            'olsrd still running on {} with olsr switched off'.format(
                node.hostname)

    status, out = node.execute('ps w | grep -c "[o]lsrd6"')
    assert out.strip() == '0', \
        'olsrd6 is running on {}, babel carries IPv6 here'.format(
            node.hostname)


def check_babel_v4(node, olsr):
    """babel redistributes local IPv4 only when nothing else routes it."""
    conf = node.succeed('cat /etc/gluon-babeld.conf')
    has_v4 = any(
        line.startswith('redistribute local ip')
        and not line.startswith('redistribute local ip 0.0.0.0/0')
        for line in conf.split('\n'))
    if olsr:
        assert not has_v4, \
            'babel redistributes IPv4 on {} while olsrd carries it'.format(
                node.hostname)
    else:
        assert has_v4, \
            'nothing carries IPv4 on {}: olsrd is off and babel does ' \
            'not redistribute it'.format(node.hostname)


a, b = chain(2)
attach_client(a)
attach_client(b)

start()

wait_connected(a, b)
wait_connected(b, a)

client = Client(a, b)
client.move_to(a)
addr = client.wait_addr()

for olsr in (True, False, True):
    for node in (a, b):
        node.dbg('olsr {}'.format('on' if olsr else 'off'))
        set_olsr(node, olsr)
        check_daemons(node, olsr)
        check_babel_v4(node, olsr)

    # The client is untouched by any of this: babel carries IPv6 in
    # every setting, so the far node keeps reaching it.
    client.wait_addr()
    b.wait_until_succeeds(
        'ping -c 3 ' + addr, CONVERGENCE_TIMEOUT[proto(b)])

    # ... and it still roams.
    client.move_to(b)
    client.wait_addr()
    a.wait_until_succeeds(
        'ping -c 3 ' + addr, CONVERGENCE_TIMEOUT[proto(a)])
    client.move_to(a)
    client.wait_addr()

finish()
