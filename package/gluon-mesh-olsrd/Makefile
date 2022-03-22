include $(TOPDIR)/rules.mk

PKG_NAME:=gluon-mesh-olsrd
PKG_VERSION=1

PKG_BUILD_DEPENDS += libjson-c

include ../gluon.mk

define Package/gluon-mesh-olsrd
  TITLE:=olsrd mesh
  DEPENDS:= \
		+gluon-core \
		+kmod-macvlan \
		@IPV6 +olsrd +oonf-olsrd2 \
		+firewall \
		+libgluonutil \
		+libjson-c \
		+olsrd-mod-jsoninfo \
		+olsrd-mod-httpinfo \
		+olsrd-mod-txtinfo \
		+liblua \
		+ip-full
  # +gluon-mmfd +libiwinfo  +libjson-c +libnl-tiny +libubus +libubox +libblobmsg-json +libbabelhelper +luabitop
  PROVIDES:=gluon-mesh-provider
endef

define Package/gluon-mesh-olsrd/install
	$(Gluon/Build/Install)

	$(INSTALL_DIR) $(1)/usr/lib/lua/gluon
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/olsrd.so $(1)/usr/lib/lua/gluon/
endef

$(eval $(call BuildPackageGluon,gluon-mesh-olsrd))
