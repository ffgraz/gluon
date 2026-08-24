include $(TOPDIR)/rules.mk

PKG_NAME:=ffgraz-config-mode-manman-sync
PKG_VERSION:=2

include $(TOPDIR)/../package/gluon.mk

define Package/ffgraz-config-mode-manman-sync
  TITLE:=Sync location data from manman
  DEPENDS:=+gluon-config-mode-core +ffgraz-manman-sync
endef

$(eval $(call BuildPackageGluon,ffgraz-config-mode-manman-sync))
