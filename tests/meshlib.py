"""Protocol- and topology-abstractions for mesh test scenarios.

Scenarios stay independent of the site config the image was built with:
topologies are built from the helpers below, and the routing protocol
is auto-detected on the running nodes, so the same scenario can run
against batman-adv, babel and olsrd images.
"""

import os
import shlex
import subprocess
import sys
import time

from pynet import SLOT, Node, connect

#: Routing protocols the rig knows how to drive.
PROTOCOLS = ('batman-adv', 'babel', 'olsrd')

#: How long a protocol may take to converge, in seconds. gluon-mesh-babel
#: registers mesh interfaces with "update-interval 300", so a route can
#: take up to five minutes to propagate; the first route between two
#: nodes was measured at ~325s, and olsrd is given the same headroom.
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
    """Ping one node from another over the mesh, retrying until it
    works or the protocol's convergence budget runs out."""
    frm.wait_until_succeeds('ping -c {} {}'.format(count, node_addr(to)),
                            CONVERGENCE_TIMEOUT[proto(frm)])


# --- uplink / internet ---

#: Address used to check IPv4 internet reachability. Overridable
#: through ``GLUON_TEST_V4_TARGET``, so a disconnected lab can point at
#: a local responder instead of a public one.
V4_TARGET = os.environ.get('GLUON_TEST_V4_TARGET', '1.1.1.1')

#: Address used to check IPv6 internet reachability
#: (``GLUON_TEST_V6_TARGET``).
V6_TARGET = os.environ.get('GLUON_TEST_V6_TARGET', '2606:4700:4700::1111')

#: Name used to check that DNS resolves (``GLUON_TEST_DNS_NAME``).
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


def uplink_routable(node, family=6):
    """True if the uplink carries an address the node can source global
    traffic from.

    Having an uplink address is not the same as being able to use it: a
    private IPv4 address still reaches the internet through the
    upstream NAT, while the deprecated fec0::/64 that QEMU's user
    networking hands out is one Linux will not pick as a source for a
    global destination."""
    if family == 4:
        cmd = 'ip -4 addr show dev br-wan | grep -q "inet "'
    else:
        cmd = 'ip -6 addr show dev br-wan | grep -qE "inet6 [23]"'
    status, _ = node.execute(cmd)
    return status == 0


def reaches_internet(node, family=6, target=None):
    """True if the node can reach the given address off-mesh."""
    target = target or (V4_TARGET if family == 4 else V6_TARGET)
    status, _ = node.execute('ping -{} -c 3 -W 5 {}'.format(family, target))
    return status == 0


def resolves_dns(node, name=None):
    """True if the node's own resolver answers for a name."""
    status, _ = node.execute('nslookup {} >/dev/null'.format(name or DNS_NAME))
    return status == 0


def as_mesh_vpn(cmd):
    """Run a command as the gluon-mesh-vpn group.

    Only that group's DNS is redirected to gluon-wan-dnsmasq, so a
    lookup has to come from it to take the path the package exists for.
    The node has no su or setpriv; busybox's start-stop-daemon can
    change the group of what it starts."""
    program, _, arguments = cmd.partition(' ')
    return ('start-stop-daemon -S -c :gluon-mesh-vpn -x "$(which {})" -- {}'
            .format(program, arguments))


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


def respondd_query(
        dev, request='nodeinfo', count=2, timeout=RESPONDD_MAXDELAY + 5):
    """Command querying the mesh-wide respondd group. respondd answers
    ff02::2:1001 on mesh devices (link-local, direct neighbours only)
    and ff05::2:1001 on the client device, which is the one carried
    across the mesh."""
    return ('gluon-neighbour-info -d ff05::2:1001 -p 1001 -r {} -i {} -c {}'
            ' -t {}'.format(request, dev, count, timeout))


# --- firewall ---

def send_from(client, scapy_expr):
    """Send a crafted packet from a client's namespace. scapy_expr is
    evaluated with scapy.all imported and IFACE bound to the client's
    interface.

    Crafted with the interpreter running the test, not with whatever
    'python3' resolves to: the two differ under sudo, or whenever the
    requirements were installed into a virtualenv, and then scapy is
    missing from the one that would have been picked."""
    prog = ('from scapy.all import *\nIFACE={!r}\n{}\n'
            .format(client.at.if_client, scapy_expr))
    return client._ns('{} -c {}'.format(
        shlex.quote(sys.executable), shlex.quote(prog)))


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
    """Frames the firewall's drop rules have counted.

    Every drop rule Gluon installs carries an explicit counter, so the
    ruleset itself says what the firewall did with a frame - there is
    no need to infer it from what reached the mesh."""
    _, out = node.execute(
        "nft list ruleset | grep -oE 'counter packets [0-9]+'"
        " | awk '{s+=$3} END {print s+0}'")
    return int(out.strip())


def flood_multicast(node):
    """Have the client bridge offer every multicast frame to every port.

    br-client snoops multicast, so a frame addressed to a group nothing
    has joined is dropped by the bridge before the firewall ever sees
    it - which looks exactly like the firewall blocking it, and drifts
    with whatever the mesh happens to have joined. Turning snooping off
    leaves the firewall as the only thing that can stop a frame, which
    is what a firewall test wants to measure."""
    node.succeed('echo 0 > /sys/class/net/br-client/bridge/multicast_snooping')


def dump_firewall(node):
    """Return the node's active firewall state. Also written to the
    node log so it is archived as a test artifact."""
    dump = node.succeed('nft list ruleset')
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
        # Carries the slot, like the client tap: the namespace is
        # host-visible, and the stale-namespace cleanup below would
        # otherwise delete the one a run beside this is using.
        self.netns = 'testclient%d_%d' % (SLOT, Client._count)
        self.mac = '02:00:00:00:%02x:01' % Client._count
        self.at = None

        self._host('ip netns del ' + self.netns, check=False)
        self._host('ip netns add ' + self.netns)
        self._ns('ip link set lo up')
        for node in nodes:
            self._host('ip link set {} netns {}'.format(node.if_client, self.netns))

    def _host(self, cmd, check=True):
        res = subprocess.run(
            cmd, shell=True, capture_output=True, text=True)
        if check and res.returncode != 0:
            # Raise with what the command said. CalledProcessError
            # captures stderr and then does not show it, which turns
            # anything failing out here into an exit status and no
            # explanation.
            raise Exception('host command failed: {}\n{}{}'.format(
                cmd, res.stdout, res.stderr))
        return res.stdout

    def _ns(self, cmd, check=True):
        return self._host('ip netns exec {} {}'.format(self.netns, cmd), check=check)

    def move_to(self, node):
        """Bring the client up behind a node, taking its client tap.

        The client keeps one MAC and one address across a move, which
        is what makes it a roam rather than a new client."""
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
        """Run a command in the client's network namespace."""
        return self._ns(cmd)
