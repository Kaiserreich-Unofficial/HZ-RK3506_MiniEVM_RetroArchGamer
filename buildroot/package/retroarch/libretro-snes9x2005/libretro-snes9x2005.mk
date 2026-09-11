################################################################################
#
# SNES9X2005
#
################################################################################
LIBRETRO_SNES9X2005_VERSION = 878f1ce501f6977e666c7a43bfd1199e228bde60
LIBRETRO_SNES9X2005_SITE = $(call github,libretro,snes9x2005,$(LIBRETRO_SNES9X2005_VERSION))

define LIBRETRO_SNES9X2005_BUILD_CMDS
	CFLAGS="$(TARGET_CFLAGS) -fcommon" CXXFLAGS="$(TARGET_CXXFLAGS) -fcommon" \
	       LDFLAGS="$(TARGET_LDFLAGS) -flto -mcpu=cortex-a7 -mfpu=neon-vfpv4" \
	       $(MAKE) -C $(@D) \
	       CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" LD="$(TARGET_CC)" \
	       RANLIB="$(TARGET_RANLIB)" AR="$(TARGET_AR)" \
	       platform="unix"
endef

define LIBRETRO_SNES9X2005_INSTALL_TARGET_CMDS
	$(INSTALL) -D $(@D)/snes9x2005_libretro.so \
		$(TARGET_DIR)/usr/lib/libretro/snes9x2005_libretro.so
endef

$(eval $(generic-package))
