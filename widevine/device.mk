PRODUCT_PACKAGES += \
	android.hardware.drm-service.clearkey \

-include vendor/widevine/libwvdrmengine/apex/device/device.mk
BOARD_VENDOR_SEPOLICY_DIRS += device/google/gs201-sepolicy/widevine