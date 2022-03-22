OLSRD
===========

Gluon support OLSRD, both version v1 and v2 in the following modes:

- olsrd
  - v4 only
- olsrd2
  - v4 only
  - v6 only
  - dual-stack

olsrdv1 support is intended mostly for migration purposes
and as such v1 IPv6 support is not going to be added

Configuration
-------------

The LAN will automatically be determined by the specified prefix and prefix6

The following options exist

.. code-block:: lua
    {
      mesh {
        olsrd = {
          v1 = {
            -- Enable v1
            -- enable = true,

            -- Set olsrd configuration
            -- config = {
            --   DebugLevel = 0,
            --   IpVersion = 4,
            --   AllowNoInt = yes,
            -- },
          },
          v2 = {
            -- Enable v2
            enable = true,

            -- Make v2 IPv6 exclusive
            -- ip6_exclusive_mode = true,

            -- Make v2 IPv4 exclusive useful (for v1 co-existence)
            -- ip4_exclusive_mode = true,

            -- Set olsrd configuration
            -- config = {
            --
            -- }
          }
        }
      }
    }

Static IP managment
-------------------
This feature is enabled as part of olsrd support

Static IP managment has the following options

.. code-block:: lua
    {
      -- Auto-assign addresses from an IPv4 range
      tmpIp4 = '10.12.23.0/16',
      tmpIp4Range = 24, -- range of tmpIp4 that should be randomized with mac
      -- Auto-assign addresses from an IPv6 range
      tmpIp6 = 'fdff:cafe:cafe:cafe:23::/64',
      tmpIp6Range = 64, -- range of tmpIp6 that should be randomized with mac
    }

Note that these addresses are intended to be temporary (TODO: should they or would dynamic4 and dynamic4IsTmp be better?)
