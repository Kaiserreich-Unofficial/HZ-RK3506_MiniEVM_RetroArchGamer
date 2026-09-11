################################################################################
#
# GAMBATTE
#
################################################################################
LIBRETRO_GAMBATTE_VERSION = 7722012ce85e56c324cb2080645347689ca379ae
LIBRETRO_GAMBATTE_SITE = $(call github,libretro,gambatte-libretro,$(LIBRETRO_GAMBATTE_VERSION))

define LIBRETRO_GAMBATTE_BUILD_CMDS
	CFLAGS="$(TARGET_CFLAGS) -O3 -fcommon -flto -mcpu=cortex-a7 -mfpu=neon-vfpv4" CXXFLAGS="$(TARGET_CXXFLAGS) -O3 -fcommon -flto -mcpu=cortex-a7 -mfpu=neon-vfpv4" \
	       LDFLAGS="$(TARGET_LDFLAGS) -flto -mcpu=cortex-a7 -mfpu=neon-vfpv4" \
	       $(MAKE) -C $(@D) -f Makefile.libretro \
	       CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" LD="$(TARGET_CC)" \
	       RANLIB="$(TARGET_RANLIB)" AR="$(TARGET_AR)" \
	       platform="$(LIBRETRO_PLATFORM)"
endef

define LIBRETRO_GAMBATTE_INSTALL_TARGET_CMDS
	$(INSTALL) -D $(@D)/gambatte_libretro.so \
		$(TARGET_DIR)/usr/lib/libretro/gambatte_libretro.so
endef

$(eval $(generic-package))
