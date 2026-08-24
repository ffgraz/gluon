include $(TOPDIR)/rules.mk

PKG_NAME:=ffgraz-manman-sync
PKG_VERSION:=1

include $(TOPDIR)/../package/gluon.mk

define Package/ffgraz-manman-sync
  DEPENDS:=+gluon-core +micrond +luci-lib-ip +ffgraz-signed-request
  TITLE:=Sync configuration with data from manman
endef

$(eval $(call BuildPackageGluon,ffgraz-manman-sync))
