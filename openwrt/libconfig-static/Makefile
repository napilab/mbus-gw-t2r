#++
#	FACILITY: A yet another gateway for MODBUS (TCP to RTU) - the OpenWrt feed
#
#	DESCRIPTION: libconfig as a static library, the gateway is linked with it. The libconfig of
#		the OpenWrt packages feed is a shared one only (--disable-static); linked statically
#		the gateway package has no dependencies and is installed with no network, as the
#		.deb is. A build-only package: nothing goes to the device. The same version and
#		tarball as the libconfig of the OpenWrt 25.12 feed.
#
#	AUTHOR: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#	CREATION DATE: 29-OCT-2026
#--

include $(TOPDIR)/rules.mk

PKG_NAME:=libconfig-static
PKG_VERSION:=1.8.1
PKG_RELEASE:=1

PKG_SOURCE:=libconfig-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://hyperrealm.github.io/libconfig/dist/
PKG_HASH:=87c6f382994b245f9213be34a2bf19c8ee7d033d7abaa51e88fbb7bad79e2dc6
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)/libconfig-$(PKG_VERSION)

PKG_LICENSE:=LGPL-2.1-or-later
PKG_LICENSE_FILES:=COPYING.LIB
PKG_MAINTAINER:=Ruslan R. Laishev <zator@yandex.ru>

PKG_INSTALL:=1
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/libconfig-static
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=Configuration File Library (a static library, build only)
  URL:=https://hyperrealm.github.io/libconfig/
  BUILDONLY:=1
endef

define Package/libconfig-static/description
 libconfig as a static library for the programs linked with it; nothing is
 installed on the device.
endef

CONFIGURE_ARGS += \
	--enable-static \
	--disable-shared \
	--disable-examples \
	--disable-cxx

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include $(1)/usr/lib/pkgconfig
	$(CP) $(PKG_INSTALL_DIR)/usr/include/libconfig.h $(1)/usr/include/
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libconfig.a $(1)/usr/lib/
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/pkgconfig/libconfig.pc $(1)/usr/lib/pkgconfig/
endef

$(eval $(call BuildPackage,libconfig-static))
