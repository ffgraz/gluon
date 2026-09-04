# olsrd v1 gateway, one daemon per address family like gluon-mesh-olsrd
# on upstream-olsr: UDP 698, Mode mesh, one address per family on every
# mesh device, jsoninfo on 9090/9091, and the default route as an HNA.
{
  config,
  lib,
  pkgs,
  site,
  ...
}:
let
  cfg = config.gateway;
  # nixpkgs builds only the daemon; the plugins are a separate make
  # target (not all of them build, and their install wants /sbin/ldconfig).
  plugins = [
    "jsoninfo"
    "txtinfo"
  ];
  olsrd = pkgs.olsrd.overrideAttrs (old: {
    preConfigure = old.preConfigure + ''
      makeFlagsArray+=("SUBDIRS=${toString plugins}")
    '';
    buildFlags = [
      "all"
      "libs"
    ];
    postInstall = lib.concatMapStringsSep "\n" (p: ''
      install -D -m 755 lib/${p}/olsrd_${p}.so.* -t $out/lib
    '') plugins;
  });
  conf =
    fam:
    pkgs.writeText "olsrd${fam}.conf" ''
      DebugLevel 0
      AllowNoInt yes
      IpVersion ${fam}
      OlsrPort 698
      LinkQualityLevel 2
      FIBMetric "flat"
      Willingness 3
      ${if fam == "4" then "Hna4 { 0.0.0.0 0.0.0.0 }" else "Hna6 { :: 0 }"}
      LoadPlugin "${olsrd}/lib/olsrd_jsoninfo.so.1.1" {
        PlParam "accept" "${if fam == "4" then "127.0.0.1" else "::1"}"
        PlParam "port" "${if fam == "4" then "9090" else "9091"}"
      }
      InterfaceDefaults {
        Mode "mesh"
        ${lib.optionalString (fam == "6") ''IPv6Src "${cfg.address6}/128"''}
      }
      Interface ${lib.concatMapStringsSep " " (d: ''"${d}"'') cfg.meshDevices} { }
    '';
  service = fam: {
    description = "olsrd (IPv${fam})";
    wantedBy = [ "multi-user.target" ];
    after = [ "network.target" ];
    serviceConfig = {
      ExecStart = "${olsrd}/bin/olsrd -f ${conf fam} -nofork";
      Restart = "always";
      RestartSec = 2;
    };
  };
in
{
  systemd.services =
    lib.optionalAttrs (cfg.address4 != null) { olsrd4 = service "4"; }
    // lib.optionalAttrs (cfg.address6 != null) { olsrd6 = service "6"; };

  systemd.network.networks = lib.listToAttrs (
    map (dev: {
      name = "30-${dev}";
      value.address =
        lib.optional (cfg.address4 != null) "${cfg.address4}/32"
        ++ lib.optional (cfg.address6 != null) "${cfg.address6}/128";
    }) cfg.meshDevices
  );
}
