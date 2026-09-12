################################################################################
#
# retro-zero
#
################################################################################

RETRO_ZERO_VERSION = 26784119de8d115e58b908706cb1f0d863e9e981
RETRO_ZERO_SITE = $(call github,geo-tp,Retro-Zero,$(RETRO_ZERO_VERSION))

# Upstream ships no LICENSE file, so this is all-rights-reserved by default.
# It is included here as a build-time dependency for this private firmware;
# see package/retro-zero/README.md.
RETRO_ZERO_LICENSE = Proprietary (no license file upstream)
RETRO_ZERO_LICENSE_FILES =

RETRO_ZERO_DEPENDENCIES = alsa-lib

# Upstream's Makefile defaults to an aarch64 cross compiler; passing CC/CXX on
# the command line takes precedence over those defaults. CPPFLAGS/CXXFLAGS/
# LDLIBS are plain assignments in the Makefile, so command-line values replace
# them outright (which is what we want: TARGET_CFLAGS carries the sysroot).
#
# CP0_WITH_EGL_FBDEV=0 is essential here: the RK3506 has no GPU, so there is no
# EGL/GLES to link against. Retro-Zero then renders through /dev/fb0 only.
RETRO_ZERO_MAKE_OPTS = \
	CC="$(TARGET_CC)" \
	CXX="$(TARGET_CXX)" \
	CPPFLAGS="$(TARGET_CPPFLAGS) -Isrc -Ivendor -Ivendor/lvgl -I. \
		-DLV_CONF_INCLUDE_SIMPLE -DCP0_HAVE_ALSA=1 -DCP0_WITH_LVGL=1 \
		-DCP0_WITH_EGL_FBDEV=0" \
	CFLAGS="$(TARGET_CFLAGS)" \
	CXXFLAGS="$(TARGET_CXXFLAGS) -std=c++17" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	LDLIBS="-ldl -lasound -lpthread -lm"

define RETRO_ZERO_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) $(RETRO_ZERO_MAKE_OPTS) -C $(@D)
endef

define RETRO_ZERO_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/build/cp0-libretro-fb0 \
		$(TARGET_DIR)/usr/bin/retro-zero
endef

# The tarball carries prebuilt libretro cores for ARM64 (used by upstream's
# Cardputer Zero target) plus LVGL's test suite and docs. None of it is
# compiled, so drop it to keep the build tree small. This board uses the
# ARM32 cores built by the libretro-* packages instead.
define RETRO_ZERO_REMOVE_UNUSED
	rm -rf $(@D)/emulators $(@D)/vendor/lvgl/tests \
		$(@D)/vendor/lvgl/demos $(@D)/vendor/lvgl/docs \
		$(@D)/vendor/lvgl/scripts
endef
RETRO_ZERO_POST_EXTRACT_HOOKS += RETRO_ZERO_REMOVE_UNUSED

ifeq ($(BR2_PACKAGE_RETRO_ZERO_CONSOLE),y)

# Registered as a POST_INSTALL_TARGET hook so it runs after the binary lands.
define RETRO_ZERO_CONSOLE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(RETRO_ZERO_PKGDIR)/console/retro-zero-forever \
		$(TARGET_DIR)/usr/bin/retro-zero-forever
	$(INSTALL) -D -m 0755 $(RETRO_ZERO_PKGDIR)/console/S99retrozero \
		$(TARGET_DIR)/etc/init.d/S99retrozero
	$(INSTALL) -D -m 0644 $(RETRO_ZERO_PKGDIR)/console/wqy-cjk.ttf \
		$(TARGET_DIR)/usr/share/fonts/wqy-cjk.ttf
	mkdir -p $(TARGET_DIR)/root/saves $(TARGET_DIR)/root/states \
		$(TARGET_DIR)/root/roms
endef

RETRO_ZERO_POST_INSTALL_TARGET_HOOKS += RETRO_ZERO_CONSOLE_INSTALL_TARGET_CMDS
endif

$(eval $(generic-package))
