include $(TOPDIR)/rules.mk

PKG_NAME:=ffgraz-monitor-and-reboot
PKG_VERSION=1

PKG_MAINTAINER:=Maciej Krüger <maciej@xeredo.it>
PKG_LICENSE:=MIT

include $(TOPDIR)/../package/gluon.mk

define Package/ffgraz-monitor-and-reboot
  TITLE:=Automatically reboot node when things go south
  DEPENDS:= +ucode +ucode-mod-ubus +ucode-mod-uloop +ucode-mod-uci
endef

$(eval $(call BuildPackageGluon,ffgraz-monitor-and-reboot))
