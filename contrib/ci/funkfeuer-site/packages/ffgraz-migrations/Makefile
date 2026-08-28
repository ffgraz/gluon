include $(TOPDIR)/rules.mk

PKG_NAME:=ffgraz-migrations
PKG_VERSION:=1

include $(TOPDIR)/../package/gluon.mk

define Package/ffgraz-migrations
  DEPENDS:=+gluon-core
  TITLE:=Graz-specific gluon migrations
endef

$(eval $(call BuildPackageGluon,ffgraz-migrations))
