# What every gateway image has: an uplink on QEMU's user network with
# NAT44/NAT66 towards it, root ssh with the key pynet passes as a systemd
# credential, a serial console, and the mesh NICs (with gluon's VXLAN on
# top when the site uses it). Protocol modules add the daemons on
# config.gateway.meshDevices.
{
  config,
  lib,
  pkgs,
  modulesPath,
  site,
  ...
}:
let
  g = import ./lib.nix { inherit lib; };
  cfg = config.gateway;
  # NICs are named after the role pynet encodes in the MAC's fourth byte
  # (see the udev rules below); unused mesh ones just never appear.
  roles = {
    "01" = "uplink";
    "02" = "client";
    "0b" = "mesh1";
    "0c" = "mesh2";
    "0d" = "mesh3";
    "0e" = "mesh4";
  };
  meshNics = lib.filter (lib.hasPrefix "mesh") (lib.attrValues roles);
  vxlan = site.mesh.vxlan or true;
  vxName = nic: "vx" + lib.removePrefix "mesh" nic;
  prefix4 = site.node_prefix4 or site.prefix4 or null;
  prefix6 = site.node_prefix6 or site.prefix6 or null;
in
{
  imports = [ (modulesPath + "/profiles/qemu-guest.nix") ];

  options.gateway = {
    site = lib.mkOption {
      type = lib.types.attrs;
      default = site;
      description = "The gluon site config, as gluon-show-site printed it.";
    };
    uplink = lib.mkOption {
      type = lib.types.str;
      default = "uplink";
      description = "NIC on QEMU's user network; the internet as far as the gateway knows.";
    };
    meshDevices = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = if vxlan then map vxName meshNics else meshNics;
      description = "Devices the mesh daemon runs on: the VXLANs, or the raw NICs when the site disables VXLAN.";
    };
    address4 = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = if prefix4 == null then null else g.gateway4 site prefix4;
      description = "The gateway's IPv4 address in the mesh; null without an IPv4 site prefix.";
    };
    address6 = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = if prefix6 == null then null else g.gateway6 site prefix6;
      description = "The gateway's IPv6 address in the mesh; null without an IPv6 site prefix.";
    };
  };

  config = {
    system.stateVersion = config.system.nixos.release;
    boot.loader.timeout = 0;
    boot.kernelParams = [ "console=ttyS0" ];
    services.getty.autologinUser = "root";
    documentation.enable = false;
    nix.enable = false;

    services.udev.extraRules = lib.concatStrings (
      lib.mapAttrsToList (byte: name: ''
        SUBSYSTEM=="net", ACTION=="add", ATTR{address}=="52:54:??:${byte}:34:??", NAME="${name}"
      '') roles
    );

    boot.kernel.sysctl = {
      "net.ipv4.conf.all.forwarding" = 1;
      "net.ipv6.conf.all.forwarding" = 1;
      "net.ipv4.conf.all.rp_filter" = lib.mkDefault 0;
      "net.ipv4.conf.default.rp_filter" = lib.mkDefault 0;
    };

    networking = {
      useNetworkd = true;
      useDHCP = false;
      firewall.enable = false;
      nftables.enable = true;
      # By interface, not by prefix: whatever the site uses internally
      # leaves through the uplink's address.
      nat = {
        enable = true;
        enableIPv6 = true;
        externalInterface = cfg.uplink;
        internalInterfaces = cfg.meshDevices ++ [ "br-client" ];
      };
    };

    systemd.network = {
      networks =
        {
          "10-uplink" = {
            matchConfig.Name = cfg.uplink;
            networkConfig = {
              DHCP = "yes";
              IPv6AcceptRA = true;
            };
          };
        }
        // lib.optionalAttrs vxlan (
          lib.listToAttrs (
            map (nic: {
              name = "20-${nic}";
              value = {
                matchConfig.Name = nic;
                networkConfig.LinkLocalAddressing = "ipv6";
                vxlan = [ (vxName nic) ];
              };
            }) meshNics
          )
        )
        // lib.listToAttrs (
          map (dev: {
            name = "30-${dev}";
            value = {
              matchConfig.Name = dev;
              networkConfig.LinkLocalAddressing = "ipv6";
            };
          }) cfg.meshDevices
        );
      netdevs = lib.optionalAttrs vxlan (
        lib.listToAttrs (
          map (nic: {
            name = "20-${vxName nic}";
            value = {
              netdevConfig = {
                Name = vxName nic;
                Kind = "vxlan";
              };
              # gluon_wired.sh: peer ff02::15c, VNI from the domain seed,
              # checksums off.
              vxlanConfig = {
                VNI = g.vni site;
                Group = "ff02::15c";
                Local = "ipv6_link_local";
                DestinationPort = 4789;
                UDP6ZeroChecksumTx = true;
                UDP6ZeroChecksumRx = true;
              };
            };
          }) meshNics
        )
      );
    };

    services.openssh = {
      enable = true;
      settings.PermitRootLogin = "yes";
    };
    # pynet passes its public key as the systemd credential
    # ssh.authorized_keys.root over SMBIOS, so the image needs no key.
    systemd.services.gateway-ssh-key = {
      wantedBy = [ "multi-user.target" ];
      before = [ "sshd.service" ];
      serviceConfig = {
        Type = "oneshot";
        ImportCredential = "ssh.authorized_keys.root";
      };
      script = ''
        key="$CREDENTIALS_DIRECTORY/ssh.authorized_keys.root"
        [ -f "$key" ] || exit 0
        mkdir -p /root/.ssh
        chmod 700 /root/.ssh
        install -m 600 "$key" /root/.ssh/authorized_keys
      '';
    };

    environment.systemPackages = with pkgs; [
      iproute2
      iputils
      tcpdump
    ];
  };
}
