# Fingerprint
include device/google/gs201/fingerprint/fpc1540/fingerprint_config.mk

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml
