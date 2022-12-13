OLSRD
===========

[todo: re-work for upstream]

Gluon supports OLSRD, both version 1 and 2 in the following modes:

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

            -- Set additional olsrd configuration
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

            -- Make v2 IPv4 exclusive (useful for v1 co-existence)
            -- ip4_exclusive_mode = true,

            -- Set additional olsrd2 configuration
            -- config = {
            --
            -- }
          }
        }
      }
    }
