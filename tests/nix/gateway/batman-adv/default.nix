# batman-adv gateway: bat0 over the mesh devices, bridged with the client
# NIC into br-client, where the gateway is the DHCPv4 and RA server for
# the mesh's clients.
{
  config,
  lib,
  site,
  ...
}:
let
  g = import ../lib.nix { inherit lib; };
  cfg = config.gateway;
  algo = lib.toLower (lib.replaceStrings [ "_" ] [ "-" ] (site.mesh.batman_adv.routing_algo or "BATMAN_IV"));
in
{
  boot.kernelModules = [ "batman_adv" ];

  systemd.network.netdevs = {
    "40-bat0" = {
      netdevConfig = {
        Name = "bat0";
        Kind = "batadv";
      };
      batmanAdvancedConfig = {
        RoutingAlgorithm = algo;
        GatewayMode = "server";
      };
    };
    "50-br-client".netdevConfig = {
      Name = "br-client";
      Kind = "bridge";
    };
  };

  systemd.network.networks =
    lib.listToAttrs (
      map (dev: {
        name = "30-${dev}";
        value.networkConfig.BatmanAdvanced = "bat0";
      }) cfg.meshDevices
    )
    // {
      "40-bat0" = {
        matchConfig.Name = "bat0";
        bridge = [ "br-client" ];
      };
      "40-client" = {
        matchConfig.Name = "client";
        bridge = [ "br-client" ];
      };
      "50-br-client" = {
        matchConfig.Name = "br-client";
        address =
          lib.optional (cfg.address4 != null) "${cfg.address4}/${toString (g.ip4Len site.prefix4)}"
          ++ lib.optional (cfg.address6 != null) "${cfg.address6}/64";
        networkConfig = {
          ConfigureWithoutCarrier = true;
          IPv6AcceptRA = false;
        };
      };
    };

  services.dnsmasq = lib.mkIf (cfg.address4 != null) {
    enable = true;
    resolveLocalQueries = false;
    settings = {
      interface = "br-client";
      bind-interfaces = true;
      no-resolv = true;
      server = [ "10.0.2.3" ]; # QEMU user network DNS
      dhcp-range = "${g.ip4Host site.prefix4 100},${g.ip4Host site.prefix4 (g.ip4Size site.prefix4 - 2)},12h";
      dhcp-option = [
        "option:router,${cfg.address4}"
        "option:dns-server,${cfg.address4}"
      ];
    };
  };

  services.radvd = lib.mkIf (cfg.address6 != null) {
    enable = true;
    config = ''
      interface br-client {
        AdvSendAdvert on;
        MinRtrAdvInterval 10;
        MaxRtrAdvInterval 30;
        AdvDefaultLifetime 900;
        prefix ${site.prefix6} { AdvOnLink on; AdvAutonomous on; };
        RDNSS ${cfg.address6} {};
      };
    '';
  };
}
