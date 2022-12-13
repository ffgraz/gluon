Static IP managment
-------------------

A hack for graz

Static IP managment has the following options

.. code-block:: lua
    {
      -- Auto-assign addresses from an IPv4 range
      node_prefix4 = '10.12.23.0/16',
      node_prefix4_range = 24, -- range of node_prefix4 that should be randomized with mac
      node_prefix4_temporary = true, -- (def: true) flag to indicate whether or not this is a temporary range that will need manual change for permanent assignments or not

      -- Auto-assign addresses from an IPv6 range
      node_prefix6 = 'fdff:cafe:cafe:cafe:23::/128',
      node_prefix6_range = 84, -- (def: 64) range of node_prefix6 that should be randomized with mac
      node_prefix6_temporary = true, -- (def: false) flag to indicate whether or not this is a temporary range that will need manual change for permanent assignments or not
    }

Note that these addresses are intended to be temporary
