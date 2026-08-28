include $(TOPDIR)/rules.mk

PKG_NAME:=ffgraz-vanilla
PKG_VERSION:=1

include $(TOPDIR)/../package/gluon.mk

define Package/ffgraz-vanilla
  DEPENDS:=+gluon-core
  TITLE:=Graz-specific vanilla scripts
endef

$(eval $(call BuildPackageGluon,ffgraz-vanilla))
