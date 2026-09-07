# <gluon/gateway>: the protocol modules matching what the gluon image has
# installed. An image with two mesh daemons gets a gateway with both.
{ lib, packages, ... }:
let
  has = name: lib.any (lib.hasPrefix name) packages;
in
{
  imports =
    lib.optional (has "gluon-mesh-batman-adv") ./batman-adv
    ++ lib.optional (has "gluon-mesh-babel") ./babel
    ++ lib.optional (has "gluon-mesh-olsrd") ./olsrd;
}
