# babel gateway: babeld on the mesh devices announcing the default route.
# Gluon nodes accept any route, so a plain ::/0 does. IPv4 only when the
# site carries it over babel (mesh.babel.import).
{
  config,
  lib,
  site,
  ...
}:
let
  cfg = config.gateway;
  v4 = cfg.address4 != null && site.mesh ? babel;
in
{
  services.babeld = {
    enable = true;
    interfaces = lib.genAttrs cfg.meshDevices (_: {
      type = "wired";
    });
    extraConfig = ''
      ipv6-subtrees true
      redistribute ip ::/0 le 0 allow
      redistribute ip ${cfg.address6}/128 allow
      ${lib.optionalString v4 ''
        redistribute ip 0.0.0.0/0 le 0 allow
        redistribute ip ${cfg.address4}/32 allow
      ''}
      redistribute deny
    '';
  };

  systemd.network.networks."05-lo" = {
    matchConfig.Name = "lo";
    address = [ "${cfg.address6}/128" ] ++ lib.optional v4 "${cfg.address4}/32";
  };
}
