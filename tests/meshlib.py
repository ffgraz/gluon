"""Protocol- and topology-abstractions for mesh test scenarios.

Scenarios stay independent of the site config the image was built with:
topologies are built from the helpers below, and the routing protocol
is auto-detected on the running nodes, so the same scenario can run
against batman-adv, babel and olsrd images.
"""

from pynet import Node, connect

PROTOCOLS = ('batman-adv', 'babel', 'olsrd')

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

# ponytail: star()/full-mesh topologies when a scenario needs them


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


def ping(frm, to, count=5):
    frm.wait_until_succeeds('ping -c {} {}'.format(count, node_addr(to)))
