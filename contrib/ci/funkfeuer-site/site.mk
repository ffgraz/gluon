##	gluon site.mk makefile example

##	GLUON_FEATURES
#		Specify Gluon features/packages to enable;
#		Gluon will automatically enable a set of packages
#		depending on the combination of features listed

GLUON_FEATURES := \
	logging \
	autoupdater \
	mesh-olsrd \
	mesh-babel \
	respondd \
	status-page \
	web-advanced \
	web-wizard \
	web-admin \
	web-private-wifi \
	web-logging \
	authorized-keys \
	config-mode-core

# GLUON_FEATURES_standard := \
#    wireless-encryption-wpa3-openssl
 
GLUON_FEATURES_big := \
   wireless-encryption-wpa3-openssl \
   p2p-support

GLUON_FEATURES_p2p := \
   p2p-support

##	GLUON_SITE_PACKAGES
#		Specify additional Gluon/OpenWrt packages to include here;
#		A minus sign may be prepended to remove a packages from the
#		selection that would be enabled by default or due to the
#		chosen feature flags

# removed as not needed
# softflowd

# removed bc of space
# -

GLUON_SITE_PACKAGES := -batman-adv \
	ffgraz-static-ip ffgraz-web-static-ip \
	ffgraz-manman-sync ffgraz-config-mode-manman-sync \
	ffgraz-config-mode-theme-funkfeuer -gluon-config-mode-theme \
	ffgraz-private-ap ffgraz-web-private-ap \
	ffgraz-migrations \
	ffgraz-ddhcpd-nextnode \
	ffgraz-ddhcpd \
	ffgraz-monitor-and-reboot \
	ffgraz-blink \
	ffgraz-olsr-auto-restart \
	ffda-gluon-usteer \
	ffac-weeklyreboot \
	ffac-ssid-changer

GLUON_SITE_PACKAGES_standard := -batman-adv \
	iwinfo mtr-nojson iperf3 \
	ffgraz-config-mode-at-runtime ffgraz-config-mode-remote \
	ffgraz-mesh-vpn-openvpn ffgraz-web-mesh-vpn-openvpn ffgraz-mesh-olsr12-openvpn

GLUON_SITE_PACKAGES_p2p := -batman-adv \
	iwinfo mtr-nojson iperf3 \
	ffgraz-config-mode-at-runtime ffgraz-config-mode-remote

GLUON_SITE_PACKAGES_big := -batman-adv \
	iwinfo mtr-nojson tcpdump iperf3 horst \
	ffgraz-config-mode-at-runtime ffgraz-config-mode-remote \
	ffgraz-olsr-public-ip ffgraz-web-olsr-public-ip \
	ffgraz-mesh-vpn-openvpn ffgraz-web-mesh-vpn-openvpn ffgraz-mesh-olsr12-openvpn \
	ffgraz-yggdrasil

##	DEFAULT_GLUON_RELEASE
#		version string to use for images
#		gluon relies on
#			opkg compare-versions "$1" '>>' "$2"
#		to decide if a version is newer or not.

DEFAULT_GLUON_RELEASE := 0.3+exp$(shell date '+%Y%m%d')

# Variables set with ?= can be overwritten from the command line

##	GLUON_RELEASE
#		call make with custom GLUON_RELEASE flag, to use your own release version scheme.
#		e.g.:
#			$ make images GLUON_RELEASE=23.42+5
#		would generate images named like this:
#			gluon-ff%site_code%-23.42+5-%router_model%.bin

GLUON_RELEASE ?= $(DEFAULT_GLUON_RELEASE)

# Default priority for updates.
GLUON_PRIORITY ?= 0

# Region code required for some images; supported values: us eu
GLUON_REGION ?= eu

# Languages to include
GLUON_LANGS ?= en de

# Do not build images for deprecated devices
GLUON_DEPRECATED ?= 0
