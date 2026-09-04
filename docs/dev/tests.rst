Integration tests
=================

Gluon ships a rig that boots real firmware images as QEMU guests, wires
them into a mesh and drives them over SSH.

A test declares the mesh it wants, calls ``start()``, states what the
mesh should do, and calls ``finish()``:

.. code-block:: python

  #!/usr/bin/env python3
  """Two directly connected nodes see each other and can ping."""
  from pynet import start, finish
  from meshlib import pair, wait_neighbours, wait_connected, ping

  a, b = pair()

  start()

  wait_neighbours(a, 1)
  wait_connected(a, b)
  ping(a, b)

  finish()

Nothing in that names a routing protocol, interface or site config, so
the same test runs against every image the rig can boot.

Where tests live
----------------

A test belongs to the package it exercises, in a ``tests/`` directory
marked with a ``.gluon_tests`` file::

  package/gluon-core/tests/.gluon_tests
  package/gluon-core/tests/connect_two.py
  package/gluon-respondd/tests/test_respondd.py

Both Gluon's own ``package/`` tree and every checked-out feed under
``packages/`` are scanned, so a package feed pulled in through
``GLUON_SITE_FEEDS`` can ship tests the same way. The marker is what
distinguishes them from the unrelated ``tests/`` directories some
upstream packages ship.

A test runs when its own package is installed on the image. If it needs
more than that, it says so in a header:

.. code-block:: python

  # requires: gluon-mesh-batman-adv

An entry may name alternatives, ``new-name|old-name``, which keeps a
test satisfiable across a package rename. Naming the owning package
there replaces the implicit requirement on it.

The runner reads the installed package list from ``<image>.packages``
beside the image, booting one node to produce it when that file is
missing or older than the image. The image decides which tests apply.

Running them
------------

.. code-block:: sh

  cd tests
  pip install -r requirements.txt
  sudo ./run.py --image ../output/images/factory/gluon-*-x86-64.img.gz
  sudo ./run.py connect_two firewall_packets   # named tests only

``--image`` takes a ``.img`` or a ``.img.gz``; a gzipped image is
unpacked beside the archive and reused until the archive changes.
``--all`` skips the probe and runs everything, and ``-j`` sets how many
tests run at once (two by default).

Root is needed for the tests that attach clients, which use network
namespaces and taps; ``tcpdump`` and ``scapy`` are needed for the
firewall packet test. Each test gets ``tests/run/<test>/``, holding its
output in ``test.log`` and the node consoles under ``logs/``. A failing
test's output is printed as well.

Images are built per site config, which is what selects the routing
protocol: ``contrib/ci/minimal-site`` builds a batman-adv one,
``contrib/ci/babel-site`` and ``contrib/ci/olsr-site`` the layer-3
ones. Keeping one built image per protocol around avoids a rebuild when
switching between them.

Writing a test
--------------

Two modules make up the API. :mod:`meshlib` states things in terms of
the mesh and detects the routing protocol on the running nodes, so a
test written against it works on any site config. :mod:`pynet` is the
layer underneath, for running a specific command on a specific node.

meshlib
^^^^^^^

.. automodule:: meshlib
  :members:

pynet
^^^^^

.. currentmodule:: pynet

.. autoclass:: Node
  :members: add_mesh_link, uci_set, set_domain, execute, execute_in_background,
            succeed, wait_until_succeeds

.. autoclass:: Gateway
  :members: addr

.. autofunction:: connect

.. autofunction:: start

.. autofunction:: finish

Gateway VMs
-----------

A test can bring NixOS VMs into the network beside the nodes, for the
things a mesh expects from the far side: a gateway that runs the mesh
daemon, hands out the default route and NATs towards the internet. They
are declared in a ``<test>.nix`` next to ``<test>.py``, an attrset of
NixOS modules, one image per attribute::

  { gateway = { imports = [ <gluon/gateway/olsrd> ]; }; }

and used in the scenario as ``gw = Gateway('gateway')``, ``connect(gw, node)``.
The modules live in ``tests/nix/gateway`` and ``<gluon/...>`` resolves
there:

``<gluon/gateway>``
  picks the protocol modules matching the packages of the image under
  test, several at once for an image with two mesh daemons.
``<gluon/gateway/batman-adv>``
  bat0 over the mesh links, bridged into ``br-client`` where the gateway
  serves DHCPv4 (dnsmasq) and router advertisements (radvd) for the
  mesh's clients.
``<gluon/gateway/babel>``
  babeld announcing the default route, IPv4 too when the site carries it
  over babel.
``<gluon/gateway/olsrd>``
  olsrd v1 as gluon-mesh-olsrd runs it: one daemon per address family on
  UDP 698, ``Mode mesh``, the default route as an HNA, jsoninfo on
  9090/9091.

Every image gets ``tests/nix/gateway/base.nix``: an uplink on QEMU's user
network with NAT44 and NAT66 towards it (by interface, so any prefix can
be used inside), root ssh with pynet's key passed as a systemd credential,
a serial console with a root shell, and the NICs named by role (``uplink``,
``client``, ``mesh1``..), the mesh ones wrapped in gluon's VXLAN unless the
site sets ``mesh.vxlan = false``. Modules see the site
config as the ``site`` argument and the image's package list as
``packages``; ``config.gateway.meshDevices`` names the devices to run a
daemon on, ``config.gateway.address4``/``address6`` the gateway's mesh
addresses (the first free host of the node prefix, overridable).

The images are built with ``nix-build`` when the scenario starts, from
``<image>.site.json`` and ``<image>.packages``, which ``run.py`` caches
next to the image by booting one node (a scenario run on its own does
the same when they are missing). nix caches the result, so only the first
run of a test on a new site pays for the build. Without ``nix-build`` on
the host, ``run.py`` skips tests that have a ``.nix``. nixpkgs is taken
from ``<nixpkgs>`` (``NIX_PATH``); the workflow installs a channel.
