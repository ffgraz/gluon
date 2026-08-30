include $(TOPDIR)/rules.mk

PKG_NAME:=gluon-mesh-layer3-common

PKG_BUILD_DEPENDS := libjson-c

include ../gluon.mk

define Package/gluon-mesh-layer3-common
  TITLE:=Layer3 common files
  DEPENDS:=+gluon-core +gluon-mmfd +gluon-state-check +firewall4 +libgluonutil +libjson-c +lua-jsonc +gluon-neighbour-info
endef

define Package/gluon-mesh-layer3-common/install
	$(Gluon/Build/Install)

	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libgluon-neighbours.so $(1)/usr/lib/
endef

# The mesh daemons' respondd providers all report the same neighbour
# table, so the merging lives here where each of them can link it.
define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/gluon-neighbours.h $(1)/usr/include/

	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libgluon-neighbours.so $(1)/usr/lib/
endef

$(eval $(call BuildPackageGluon,gluon-mesh-layer3-common))
