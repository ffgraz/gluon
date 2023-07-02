include $(TOPDIR)/rules.mk

PKG_NAME:=gluon-p2p-support

include ../gluon.mk

define Package/gluon-p2p-support
  DEPENDS:=+wpa-supplicant-mini
  TITLE:=Package for supporting P2P meshing
endef

define Package/gluon-p2p-support/install
	$(INSTALL_DIR) $(1)/lib/gluon/features

	touch $(1)/lib/gluon/features/p2p
	touch $(1)/lib/gluon/features/p2p
endef

$(eval $(call BuildPackageGluon,gluon-p2p-support))
