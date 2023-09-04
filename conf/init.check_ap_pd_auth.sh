#!/vendor/bin/sh

# This script sets up `ro.vendor.sjtag_ap_is_unlocked` for the non-fused
# device. For a fused device, this property should be set by
# betterbug->ss-restart-detector when PD is acquired.

SJTAG_STATUS=0x$(cat /sys/devices/platform/sjtag_ap/interface/status)
SOFT_LOCK_BIT=4
AUTH_PASS_BIT=8

# Unlocked or locked but auth passed.
if test "$((SJTAG_STATUS & (1 << SOFT_LOCK_BIT)))" = 0 -o \
    "$((SJTAG_STATUS & (1 << AUTH_PASS_BIT)))" != 0 ; then
  setprop ro.vendor.sjtag_ap_is_unlocked true
fi
