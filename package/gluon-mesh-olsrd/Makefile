include $(TOPDIR)/rules.mk

PKG_NAME:=gluon-mesh-olsrd
PKG_VERSION=1

PKG_BUILD_DEPENDS += libjson-c libolsrdhelper

include ../gluon.mk

define Package/gluon-mesh-olsrd
	TITLE:=olsrd mesh
	DEPENDS:= \
		+gluon-core \
		+kmod-macvlan \
		+olsrd \
		+firewall4 \
		+libgluonutil \
		+libjson-c \
		+libolsrdhelper \
		+olsrd-macd \
		+olsrd-mod-jsoninfo \
		+olsrd-mod-httpinfo \
		+olsrd-mod-txtinfo \
		+ip-full \
		+gluon-mesh-layer3-common \
		+gluon-l3roamd \
		+gluon-state-check
	# No PROVIDES:=gluon-mesh-provider here. apk treats two packages
	# providing the same virtual as conflicting, which is what kept
	# this from being installed next to gluon-mesh-babel. Nothing in
	# gluon, the site or the feed depends on the virtual, and babel is
	# the mesh provider on a hybrid image - olsrd only adds IPv4.
endef

define Package/gluon-mesh-olsrd/install
	$(Gluon/Build/Install)

	$(INSTALL_DIR) $(1)/usr/lib/respondd
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/respondd.so $(1)/usr/lib/respondd/$(PKG_NAME).so

	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/olsrd-debug $(1)/usr/bin/olsrd-debug
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/olsr-respondd $(1)/usr/bin/olsr-respondd

ifdef CONFIG_GLUON_BASE
	rm -rf $(1)/lib/gluon/upgrade/
endif
endef

$(eval $(call BuildPackageGluon,gluon-mesh-olsrd))
