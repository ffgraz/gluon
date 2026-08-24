"""Protocol- and topology-abstractions for mesh test scenarios.

Scenarios stay independent of the site config the image was built with:
topologies are built from the helpers below, and the routing protocol
is auto-detected on the running nodes, so the same scenario can run
against batman-adv, babel and olsrd images.
"""

import os
import shlex
import subprocess
import time

from pynet import Node, connect

PROTOCOLS = ('batman-adv', 'babel', 'olsrd')

# gluon-mesh-babel registers mesh interfaces with "update-interval 300",
# so a route can take up to five minutes to propagate; measured ~325s for
# the first route between two nodes. olsrd is given the same headroom.
CONVERGENCE_TIMEOUT = {'batman-adv': 180, 'babel': 480, 'olsrd': 480}

_DETECT = {
    'batman-adv': 'batctl',
    'babel': 'babeld',
    'olsrd': 'olsrd2',
}

_proto = None


# --- topologies ---

def pair():
    """Two directly connected nodes."""
    a, b = Node(), Node()
    connect(a, b)
    return a, b


def chain(n):
    """n nodes connected in a line: 1 - 2 - ... - n."""
    nodes = [Node() for _ in range(n)]
    for x, y in zip(nodes, nodes[1:]):
        connect(x, y)
    return nodes


def full_mesh(n):
    """n nodes, every pair directly connected."""
    nodes = [Node() for _ in range(n)]
    for i, x in enumerate(nodes):
        for y in nodes[i + 1:]:
            connect(x, y)
    return nodes


# --- protocol abstractions ---

def proto(node):
    """Routing protocol of the image, detected once on a running node."""
    global _proto
    if _proto is None:
        for name, binary in _DETECT.items():
            status, _ = node.execute('which ' + binary)
            if status == 0:
                _proto = name
                break
        else:
            raise Exception('could not detect mesh routing protocol')
        node.dbg('detected mesh protocol: ' + _proto)
    return _proto


def wait_neighbours(node, count):
    """Wait until the routing protocol on node sees >= count neighbours."""
    cmds = {
        'batman-adv': '[ "$(batctl n -H | grep -c .)" -ge {} ]',
        'babel': '[ "$(echo dump | nc ::1 33123 | grep -c \'add neighbour\')" -ge {} ]',
        'olsrd': '[ "$(echo \'nhdpinfo link\' | nc ::1 2009 | grep -c fe80)" -ge {} ]',
    }
    node.wait_until_succeeds(cmds[proto(node)].format(count))


def node_addr(node):
    """An address of node reachable over the mesh."""
    if proto(node) == 'batman-adv':
        return node.hostname  # pynet's /etc/hosts entries
    return node.succeed('uci get network.loopback.ip6addr | cut -d/ -f1')


def wait_connected(frm, to):
    """Wait until frm knows to at the routing layer (any hop count):
    a batman-adv originator entry or a babel/olsrd route to its node
    address."""
    p = proto(frm)
    timeout = CONVERGENCE_TIMEOUT[p]
    if p == 'batman-adv':
        mac = to.succeed('cat /sys/class/net/primary0/address')
        frm.wait_until_succeeds("batctl o -H | grep -q '{}'".format(mac), timeout)
    elif p == 'babel':
        frm.wait_until_succeeds(
            "echo dump | nc ::1 33123 | grep -q 'add route.*{}'".format(node_addr(to)),
            timeout)
    else:  # olsrd
        frm.wait_until_succeeds(
            "echo 'olsrv2info routing' | nc ::1 2009 | grep -q '{}'".format(node_addr(to)),
            timeout)


def wait_all_connected(nodes):
    """Wait until every node knows every other node."""
    for a in nodes:
        for b in nodes:
            if a is not b:
                wait_connected(a, b)


def ping(frm, to, count=5):
    frm.wait_until_succeeds('ping -c {} {}'.format(count, node_addr(to)),
                            CONVERGENCE_TIMEOUT[proto(frm)])


# --- uplink / internet ---

# Overridable so a disconnected lab can point at a local responder
# instead of a public one.
V4_TARGET = os.environ.get('GLUON_TEST_V4_TARGET', '1.1.1.1')
V6_TARGET = os.environ.get('GLUON_TEST_V6_TARGET', '2606:4700:4700::1111')
DNS_NAME = os.environ.get('GLUON_TEST_DNS_NAME', 'one.one.one.one')


def wait_uplink(node, family=6):
    """Wait until the node's uplink has an address and a default route."""
    if family == 4:
        node.wait_until_succeeds('ip -4 addr show dev br-wan | grep -q "inet "')
        node.wait_until_succeeds(
            'ip -4 route show table all | grep "^default" | grep -q br-wan')
    else:
        # any non-link-local address counts; QEMU's user networking
        # hands out fec0::/64
        node.wait_until_succeeds(
            'ip -6 addr show dev br-wan | grep inet6 | grep -qv fe80')
        # gluon keeps uplink routes in a separate policy table
        node.wait_until_succeeds(
            'ip -6 route show table all | grep "^default" | grep -q br-wan')


def reaches_internet(node, family=6, target=None):
    """True if the node can reach the given address off-mesh."""
    target = target or (V4_TARGET if family == 4 else V6_TARGET)
    status, _ = node.execute('ping -{} -c 3 -W 5 {}'.format(family, target))
    return status == 0


def resolves_dns(node, name=None):
    status, _ = node.execute('nslookup {} >/dev/null'.format(name or DNS_NAME))
    return status == 0


# --- firewall ---

# --- respondd ---

def respondd_dev(node):
    """The device respondd serves the site-local group on.
    /lib/gluon/respondd/client.dev names a uci network interface
    ('client' for batman-adv, 'mmfd' for the layer-3 protocols), which
    has to be resolved to a device the way gluon-respondd's init does."""
    return node.succeed(
        'ubus call network.interface dump | jsonfilter -e '
        '"@.interface[@.interface=\'$(cat /lib/gluon/respondd/client.dev)\''
        ' && @.up=true].device"').strip()


# gluon-respondd serves the client device with '-t 10', so it spreads
# replies over up to ten seconds to avoid a response storm, while
# gluon-neighbour-info gives up after three by default. Querying with
# the default timeout therefore loses most replies.
RESPONDD_MAXDELAY = 10


def respondd_query(dev, request='nodeinfo', count=2,
                   timeout=RESPONDD_MAXDELAY + 5):
    """Command querying the mesh-wide respondd group. respondd answers
    ff02::2:1001 on mesh devices (link-local, direct neighbours only)
    and ff05::2:1001 on the client device, which is the one carried
    across the mesh."""
    return ('gluon-neighbour-info -d ff05::2:1001 -p 1001 -r {} -i {} -c {}'
            ' -t {}'.format(request, dev, count, timeout))


def send_from(client, scapy_expr):
    """Send a crafted packet from a client's namespace. scapy_expr is
    evaluated with scapy.all imported and IFACE bound to the client's
    interface."""
    prog = ('from scapy.all import *\nIFACE={!r}\n{}\n'
            .format(client.at.if_client, scapy_expr))
    return client._ns("python3 -c {}".format(shlex.quote(prog)))


def capture_while(client, pcap_filter, action, seconds=4):
    """Capture on a client's interface while action() runs, and return
    the number of matching packets that arrived."""
    pcap = '/tmp/{}.pcap'.format(client.netns)
    subprocess.run('rm -f ' + pcap, shell=True, check=False)
    tcpdump = subprocess.Popen(
        ['ip', 'netns', 'exec', client.netns, 'tcpdump', '-p', '-U',
         '-i', client.at.if_client, '-w', pcap, pcap_filter],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(1.5)  # let tcpdump attach before anything is sent
        action()
        time.sleep(seconds)
    finally:
        tcpdump.terminate()
        tcpdump.wait()

    out = subprocess.run(
        ['tcpdump', '-nn', '-r', pcap], capture_output=True, text=True,
        check=False).stdout
    return len([line for line in out.splitlines() if line.strip()])


def drop_counters(node):
    """Total packets the firewall's drop rules have counted.

    Both backends can be read the same way: ebtables counts every rule,
    and gluon's nftables drop rules carry an explicit counter. Returns a
    single total - the test compares a delta, so it does not matter
    which rule caught the packet."""
    _, out = node.execute(
        "nft list ruleset 2>/dev/null | grep -oE 'counter packets [0-9]+'"
        " | awk '{s+=$3} END {print s+0}'")
    total = int(out.strip() or 0)
    if total:
        return total
    # ebtables backend: -Lc prints "-- pcnt N -- bcnt M" per rule
    for binary in ('ebtables', 'ebtables-tiny'):
        _, out = node.execute(
            "{} -t filter -L --Lc 2>/dev/null"
            " | grep -oE 'pcnt = [0-9]+' | awk '{{s+=$3}} END {{print s+0}}'"
            .format(binary))
        if out.strip() and out.strip() != '0':
            return int(out.strip())
    return 0


def mesh_tx(node):
    """Packets transmitted on the node's mesh interfaces, to tell
    whether a frame actually left towards the mesh."""
    _, out = node.execute(
        'for d in $(gluon-list-mesh-interfaces); do'
        ' cat /sys/class/net/$d/statistics/tx_packets 2>/dev/null; done'
        " | awk '{s+=$1} END {print s+0}'")
    return int(out.strip() or 0)


def dump_firewall(node):
    """Return the node's active firewall state (nft ruleset plus the
    ebtables tables if that backend is present), for characterization
    across the ebtables->nftables migration. Also written to the node
    log so it is archived as a test artifact."""
    parts = []
    for label, cmd in (
        ('nft', 'nft list ruleset 2>/dev/null'),
        ('ebtables-filter', 'ebtables -t filter -L 2>/dev/null'),
        ('ebtables-nat', 'ebtables -t nat -L 2>/dev/null'),
    ):
        _, out = node.execute(cmd)
        if out.strip():
            parts.append('### {}\n{}'.format(label, out))
    dump = '\n'.join(parts)
    node.dbg('firewall dump:\n' + dump)
    return dump


# --- attached clients ---

def attach_client(node):
    """Give node a host tap on its client interface, so a Client can
    attach to it. Must be called before start(); requires root."""
    if os.geteuid() != 0:
        raise Exception('client taps require running the scenario as root')
    node.client_tap = True


class Client:
    """A simulated client in its own network namespace.

    Takes the client taps of the given nodes; move_to(node) brings the
    client (one MAC, one SLAAC address) up behind that node, like a
    device roaming through the mesh.
    """

    _count = 0

    def __init__(self, *nodes):
        Client._count += 1
        self.netns = 'testclient%d' % Client._count
        self.mac = '02:00:00:00:%02x:01' % Client._count
        self.at = None

        self._host('ip netns del ' + self.netns, check=False)
        self._host('ip netns add ' + self.netns)
        self._ns('ip link set lo up')
        for node in nodes:
            self._host('ip link set {} netns {}'.format(node.if_client, self.netns))

    def _host(self, cmd, check=True):
        return subprocess.run(cmd, shell=True, check=check,
                              capture_output=True, text=True).stdout

    def _ns(self, cmd, check=True):
        return self._host('ip netns exec {} {}'.format(self.netns, cmd), check=check)

    def move_to(self, node):
        if self.at is not None:
            self._ns('ip -6 addr flush dev ' + self.at.if_client)
            self._ns('ip link set {} down'.format(self.at.if_client))

        tap = node.if_client
        self._ns('ip link set {} address {} down'.format(tap, self.mac))
        self._ns('ip link set {} up'.format(tap))
        self.at = node

    def wait_addr(self, timeout=60):
        """Wait for the SLAAC address on the current node's tap."""
        for _ in range(timeout):
            out = self._ns('ip -6 addr show dev {} scope global'.format(self.at.if_client))
            for line in out.split('\n'):
                line = line.strip()
                if line.startswith('inet6 ') and 'tentative' not in line:
                    return line.split()[1].split('/')[0]
            time.sleep(1)
        raise Exception('client got no SLAAC address on ' + self.at.hostname)

    def succeed(self, cmd):
        return self._ns(cmd)
