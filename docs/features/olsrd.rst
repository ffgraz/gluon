OLSRD
===========

[todo: re-work for upstream]

Gluon supports olsrd (OLSR version 1), which runs one daemon per address
family:

- ``olsrd`` for IPv4, started when the site sets ``prefix4``
- ``olsrd6`` for IPv6, started when the site sets ``prefix6``

There is nothing to enable, the prefixes of the site (or domain) decide
which daemons run.

Configuration
-------------

The LAN will automatically be determined by the specified prefix and prefix6

Both daemons are configured by Gluon, the site configuration can add to (and
override) that configuration per address family. The address family itself
(``IpVersion``) is always set by Gluon.

.. code-block:: lua

    {
      mesh = {
        olsrd = {
          v4 = {
            -- Additional olsrd configuration for IPv4
            -- config = {
            --   DebugLevel = 0,
            --   AllowNoInt = 'yes',
            -- },
          },
          v6 = {
            -- Additional olsrd configuration for IPv6
            -- config = {
            -- },
          },
        },
      },
    }

Neighbour MACs
--------------

olsrd only knows the addresses of its neighbours, never their MACs - it reads
OLSR packets from UDP sockets, so the link layer never reaches it. Gluon
identifies neighbours by MAC though, so ``olsr-macd`` listens to the OLSR
traffic on a packet socket, remembers which MAC an address was last seen with
and answers over ``/var/run/olsr-macd.sock``::

    echo dump | nc -U /var/run/olsr-macd.sock
    echo 'resolve mesh_vpn 10.12.11.1' | nc -U /var/run/olsr-macd.sock

VLAN tagged OLSR traffic is not picked up, the same limitation olsrds own
arprefresh plugin has.

Querying olsrd
--------------

Both daemons load the jsoninfo plugin, IPv4 on ``127.0.0.1:9090`` and IPv6 on
``[::1]:9091``. ``olsrd-cli`` queries them::

    olsrd-cli info
    olsrd-cli olsr4 nodeinfo links
    olsrd-cli olsr6 neigh
