"""Protocol- and topology-abstractions for mesh test scenarios.

Topologies come from the helpers below and the routing protocol is
detected on the running nodes, so a scenario runs unchanged against
batman-adv, babel and olsrd images.
"""

import os
import shlex
import subprocess
import sys
import time

from pynet import SLOT, Node, Gateway, connect

#: Routing protocols the rig knows how to drive.
PROTOCOLS = ('batman-adv', 'babel', 'olsrd')

#: How long a protocol may take to converge, in seconds. gluon-mesh-babel
#: registers mesh interfaces with "update-interval 300", so a route can
#: take up to five minutes to propagate; the first route between two
#: nodes was measured at ~325s, and olsrd is given the same headroom.
#: olsrd is olsrd v1 as gluon-mesh-olsrd runs it: one daemon per address
#: family, jsoninfo on 127.0.0.1:9090 and [::1]:9091.
CONVERGENCE_TIMEOUT = {'batman-adv': 180, 'babel': 480, 'olsrd': 480}

_DETECT = {
    'batman-adv': 'batctl',
    'babel': 'babeld',
    'olsrd': 'olsrd',
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
    """Routing protocol of the image, detected once on a running node.

    An image can carry more than one - babel and olsrd run side by side
    on a hybrid image, where babel is the mesh and olsrd only adds the
    older IPv4 network - so the first match wins and GLUON_TEST_PROTO
    overrides the detection."""
    global _proto
    if _proto is None and os.environ.get('GLUON_TEST_PROTO'):
        _proto = os.environ['GLUON_TEST_PROTO']
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


#: olsrd v1 answers jsoninfo as a single JSON line, one daemon per
#: address family: IPv4 on 9090, IPv6 on 9091. Whichever runs answers.
_OLSR_JSONINFO = (
    '{{ (echo /{0}; sleep 1) | nc 127.0.0.1 9090;'
    ' (echo /{0}; sleep 1) | nc ::1 9091; }} 2>/dev/null')


def wait_neighbours(node, count):
    """Wait until the routing protocol on node sees >= count neighbours."""
    cmds = {
        'batman-adv': '[ "$(batctl n -H | grep -c .)" -ge {} ]',
        'babel': '[ "$(echo dump | nc ::1 33123 | grep -c \'add neighbour\')" -ge {} ]',
        'olsrd': (
            '[ "$(' + _OLSR_JSONINFO.format('links')
            + ' | tr , "\\n" | grep -c remoteIP)" -ge {} ]'),
    }
    p = proto(node)
    node.wait_until_succeeds(cmds[p].format(count), CONVERGENCE_TIMEOUT[p])


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
        # Not babeld's own dump: it lists a route it has retracted, and
        # one it has not installed, the same way as a usable one. The
        # kernel is the one that has to be able to reach the node.
        frm.wait_until_succeeds(
            "ip -6 route get {} 2>/dev/null | grep -qv unreachable".format(
                node_addr(to)),
            timeout)
    else:  # olsrd
        frm.wait_until_succeeds(
            _OLSR_JSONINFO.format('routes')
            + " | grep -q '{}'".format(node_addr(to)),
            timeout)


def wait_default_route(node, family=6):
    """Wait for a default route learned through the mesh: one in the
    main table that does not point at the uplink. (An uplink's IPv6
    default lives in table 1 anyway; its IPv4 one is skipped by name.)"""
    node.wait_until_succeeds(
        'ip -{} route show default | grep -v br-wan | grep -q default'.format(family),
        CONVERGENCE_TIMEOUT[proto(node)])


def wait_gateway(node, family=6):
    """Wait until node has an internet gateway through the mesh. A
    layer-3 node learns a default route from the routing protocol; a
    batman-adv node is a bridge and does not route for itself (accept_ra
    is off on br-client), it selects a batman-adv gateway instead."""
    if proto(node) == 'batman-adv':
        node.wait_until_succeeds('batctl gwl -H | grep -q .',
                                 CONVERGENCE_TIMEOUT['batman-adv'])
    else:
        wait_default_route(node, family)


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

#: IPv4 internet reachability target (``GLUON_TEST_V4_TARGET``).
V4_TARGET = os.environ.get('GLUON_TEST_V4_TARGET', '1.1.1.1')

#: IPv6 internet reachability target (``GLUON_TEST_V6_TARGET``).
V6_TARGET = os.environ.get('GLUON_TEST_V6_TARGET', '2606:4700:4700::1111')

#: Name used to check DNS resolution (``GLUON_TEST_DNS_NAME``).
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
    """True if the uplink has an address usable as source for global
    traffic. Private IPv4 works through the upstream NAT; the deprecated
    fec0::/64 from QEMU's user networking is never picked as a source."""
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
    """Run a command as the gluon-mesh-vpn group, the only one whose DNS
    is redirected to gluon-wan-dnsmasq. The node has no su or setpriv,
    but start-stop-daemon can change the group."""
    program, _, arguments = cmd.partition(' ')
    return ('start-stop-daemon -S -c :gluon-mesh-vpn -x "$(which {})" -- {}'
            .format(program, arguments))


# --- respondd ---

def respondd_dev(node):
    """The device respondd serves the site-local group on:
    /lib/gluon/respondd/client.dev names a uci interface ('client' on
    batman-adv, 'mmfd' on the layer-3 protocols), resolved to a device
    the way gluon-respondd's init does."""
    return node.succeed(
        'ubus call network.interface dump | jsonfilter -e '
        '"@.interface[@.interface=\'$(cat /lib/gluon/respondd/client.dev)\''
        ' && @.up=true].device"').strip()


# gluon-respondd spreads replies over up to 10 s (-t 10);
# gluon-neighbour-info's default 3 s timeout would lose most of them.
RESPONDD_MAXDELAY = 10


def respondd_query(
        dev, request='nodeinfo', count=2, timeout=RESPONDD_MAXDELAY + 5):
    """Command querying the mesh-wide respondd group ff05::2:1001;
    ff02::2:1001 on the mesh devices only reaches direct neighbours."""
    return ('gluon-neighbour-info -d ff05::2:1001 -p 1001 -r {} -i {} -c {}'
            ' -t {}'.format(request, dev, count, timeout))


# --- firewall ---

def send_from(client, scapy_expr):
    """Send a crafted packet from a client's namespace. scapy_expr runs
    with scapy.all imported and IFACE bound to the client's interface,
    under sys.executable: 'python3' may lack scapy under sudo or outside
    a virtualenv."""
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
    """Frames the firewall's drop rules have counted: every drop rule
    Gluon installs carries a counter, so the ruleset says what became
    of a frame."""
    _, out = node.execute(
        "nft list ruleset | grep -oE 'counter packets [0-9]+'"
        " | awk '{s+=$3} END {print s+0}'")
    return int(out.strip())


def flood_multicast(node):
    """Disable multicast snooping on br-client: otherwise frames to a
    group nothing has joined are dropped before the firewall sees them,
    which looks exactly like a firewall drop."""
    node.succeed('echo 0 > /sys/class/net/br-client/bridge/multicast_snooping')


def dump_firewall(node):
    """The nft ruleset, also written to the node log."""
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
        # Carries the slot like the client tap; the cleanup below would
        # otherwise delete a parallel run's namespace.
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
            # CalledProcessError hides captured stderr; include it
            raise Exception('host command failed: {}\n{}{}'.format(
                cmd, res.stdout, res.stderr))
        return res.stdout

    def _ns(self, cmd, check=True):
        return self._host('ip netns exec {} {}'.format(self.netns, cmd), check=check)

    def move_to(self, node):
        """Bring the client up behind a node, keeping its MAC and
        address, which makes the move a roam."""
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

    def wait_until_succeeds(self, cmd, timeout=180):
        """Run a command in the namespace once a second until it works."""
        self.at.dbg('client {}: waiting until "{}" succeeds'.format(self.netns, cmd))
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            res = subprocess.run('ip netns exec {} {}'.format(self.netns, cmd),
                                 shell=True, capture_output=True, text=True)
            if res.returncode == 0:
                return res.stdout
            time.sleep(1)
        return self._ns(cmd)

    def dhcp4(self, timeout=60):
        """Get an IPv4 lease on the current tap and install it, with the
        offered router as default. Done with scapy (already a test
        dependency) rather than a DHCP client the host may not have."""
        prog = (
            'from scapy.all import *\n'
            'import sys\n'
            'conf.checkIPaddr = False\n'
            'mac = {mac!r}\n'
            'disc = (Ether(dst="ff:ff:ff:ff:ff:ff", src=mac)/IP(src="0.0.0.0", dst="255.255.255.255")'
            '/UDP(sport=68, dport=67)/BOOTP(chaddr=mac2str(mac), xid=0x4242)'
            '/DHCP(options=[("message-type", "discover"), "end"]))\n'
            'offer = srp1(disc, iface={iface!r}, timeout={timeout}, verbose=0)\n'
            'if offer is None: sys.exit("no DHCP offer")\n'
            'opts = dict(o for o in offer[DHCP].options if isinstance(o, tuple))\n'
            'req = (Ether(dst="ff:ff:ff:ff:ff:ff", src=mac)/IP(src="0.0.0.0", dst="255.255.255.255")'
            '/UDP(sport=68, dport=67)/BOOTP(chaddr=mac2str(mac), xid=0x4242)'
            '/DHCP(options=[("message-type", "request"), ("requested_addr", offer[BOOTP].yiaddr),'
            ' ("server_id", opts["server_id"]), "end"]))\n'
            'ack = srp1(req, iface={iface!r}, timeout={timeout}, verbose=0)\n'
            'if ack is None: sys.exit("no DHCP ack")\n'
            'import ipaddress\n'
            'plen = ipaddress.IPv4Network("0.0.0.0/" + opts["subnet_mask"]).prefixlen\n'
            'print(offer[BOOTP].yiaddr, plen, opts["router"])\n'
        ).format(mac=self.mac, iface=self.at.if_client, timeout=timeout)
        out = self._ns('{} -c {}'.format(sys.executable, shlex.quote(prog))).split()
        addr, plen, router = out
        self._ns('ip -4 addr replace {}/{} dev {}'.format(addr, plen, self.at.if_client))
        self._ns('ip -4 route replace default via {} dev {}'.format(router, self.at.if_client))
        return addr, router
