PRODUCT_PACKAGES += \
	aocd

BOARD_VENDOR_SEPOLICY_DIRS += device/google/gs201-sepolicy/aoc

ifeq (,$(filter aosp_%,$(TARGET_PRODUCT)))
# IAudioMetricExt HIDL
PRODUCT_PACKAGES += \
    vendor.google.audiometricext@1.0-service-vendor
endif

# If AoC Daemon is not present on this build, load firmware at boot via rc
ifeq ($(wildcard vendor/google/whitechapel/aoc/aocd),)
PRODUCT_COPY_FILES += \
	device/google/gs201/aoc/conf/init.aoc.nodaemon.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.aoc.rc
else
PRODUCT_COPY_FILES += \
	device/google/gs201/aoc/conf/init.aoc.daemon.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.aoc.rc
endif

# AoC debug support
PRODUCT_PACKAGES_DEBUG += \
	aocdump \
	aocutil \
	aoc_audio_cfg \
	vp_util
