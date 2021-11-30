#
# Copyright (C) 2021 The Android Open-Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_WIFI_PATH := device/google/gs201/wifi/qcom

# wlan flags
BOARD_WLAN_DEVICE := qcwcn
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_HOSTAPD_DRIVER := NL80211
WPA_SUPPLICANT_VERSION := VER_0_8_X
CONFIG_IEEE80211AX := true
# BOARD_HAS_QCOM_WLAN := true
# CONFIG_ACS := true

# Wifi interface combination - {1 STA + 1 AP (bridged or single)} or {2 STA + 1 of (P2P or NAN)}
# WIFI_HAL_INTERFACE_COMBINATIONS := {{{STA}, 1}, {{AP}, 1}}, {{{STA}, 2}, {{P2P, NAN}, 1}}
WIFI_HIDL_FEATURE_AWARE := true
#Enable STA + SAP Concurrency.
WIFI_HIDL_FEATURE_DUAL_INTERFACE:= true
WIFI_FEATURE_WIFI_EXT_HAL := true
WIFI_FEATURE_IMU_DETECTION := true
WIFI_HIDL_UNIFIED_SUPPLICANT_SERVICE_RC_ENTRY := true
# Avoid Wifi reset on MAC Address change
# WIFI_AVOID_IFACE_RESET_MAC_CHANGE := true
# WIFI_FEATURE_HOSTAPD_11AX := true

# WLAN driver configuration files
PRODUCT_COPY_FILES += \
    $(LOCAL_WIFI_PATH)/wpa_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant_overlay.conf \
    $(LOCAL_WIFI_PATH)/p2p_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant_overlay.conf





