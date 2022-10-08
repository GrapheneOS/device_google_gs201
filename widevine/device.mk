PRODUCT_PACKAGES += \
	android.hardware.drm-service.clearkey \
	com.google.android.widevine
BOARD_VENDOR_SEPOLICY_DIRS += device/google/gs201-sepolicy/widevine

# Check if we can use dev keys
ifneq ($(wildcard vendor/google/dev-keystore),)
$(call soong_config_set,widevine,use_devkey,true)
endif