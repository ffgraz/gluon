include $(TOPDIR)/rules.mk

PKG_NAME:=gluon-web-mesh-olsrd

include ../gluon.mk

define Package/gluon-web-mesh-olsrd
  DEPENDS:=+gluon-web-admin +gluon-mesh-olsrd
  TITLE:=UI for enabling the olsrd mesh
endef

$(eval $(call BuildPackageGluon,gluon-web-mesh-olsrd))
