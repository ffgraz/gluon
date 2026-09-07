# Site config helpers shared by the gateway modules.
{ lib }:
rec {
  # gluon.util.domain_seed_bytes('gluon-mesh-vxlan', 3): the first md5
  # round already yields the 3 bytes.
  vni =
    site:
    lib.fromHexString (
      builtins.substring 0 6 (
        builtins.hashString "md5" ("gluon-mesh-vxlan" + lib.toLower site.domain_seed + "1")
      )
    );

  pow2 = n: lib.foldl (a: _: a * 2) 1 (lib.range 1 n);

  ip4ToInt = s: lib.foldl (a: o: a * 256 + lib.toInt o) 0 (lib.splitString "." s);
  intToIp4 =
    n:
    lib.concatMapStringsSep "." toString [
      (n / 16777216)
      (lib.mod (n / 65536) 256)
      (lib.mod (n / 256) 256)
      (lib.mod n 256)
    ];
  parse4 =
    p:
    let
      parts = lib.splitString "/" p;
    in
    {
      addr = ip4ToInt (lib.head parts);
      len = lib.toInt (lib.last parts);
    };
  ip4Len = p: (parse4 p).len;
  ip4Size = p: pow2 (32 - (parse4 p).len);
  ip4Host = p: host: intToIp4 ((parse4 p).addr + host);

  # gluon's check_site only allows /64 prefixes, written 'xxxx::/64'.
  ip6Net =
    p:
    let
      net = lib.removeSuffix "::/64" p;
    in
    assert lib.assertMsg (net != p) "IPv6 prefix ${p} is not of the form xxxx::/64";
    net;
  ip6Host = p: host: "${ip6Net p}::${host}";

  # First host of a prefix, or the second when next_node sits on the first.
  gateway4 =
    site: p:
    let
      first = ip4Host p 1;
    in
    if (site.next_node.ip4 or null) == first then ip4Host p 2 else first;
  gateway6 =
    site: p:
    let
      first = ip6Host p "1";
    in
    if (site.next_node.ip6 or null) == first then ip6Host p "2" else first;
}
