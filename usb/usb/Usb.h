/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <android-base/file.h>
#include <aidl/android/hardware/usb/BnUsb.h>
#include <aidl/android/hardware/usb/BnUsbCallback.h>
#include <aidl/android/hardware/usb/ext/BnUsbExt.h>
#include <pixelusb/UsbOverheatEvent.h>
#include <utils/Log.h>
#include <UsbDataSessionMonitor.h>

#define UEVENT_MSG_LEN 2048
// The type-c stack waits for 4.5 - 5.5 secs before declaring a port non-pd.
// The -partner directory would not be created until this is done.
// Having a margin of ~3 secs for the directory and other related bookeeping
// structures created and uvent fired.
#define PORT_TYPE_TIMEOUT 8

namespace aidl {
namespace android {
namespace hardware {
namespace usb {

using ::aidl::android::hardware::usb::IUsbCallback;
using ::aidl::android::hardware::usb::PortRole;
using ::android::base::ReadFileToString;
using ::android::base::WriteStringToFile;
using ::android::hardware::google::pixel::usb::UsbOverheatEvent;
using ::android::hardware::google::pixel::usb::ZoneInfo;
using ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V2_0::ThrottlingSeverity;
using ::android::sp;
using ::android::status_t;
using ::ndk::ScopedAStatus;
using ::std::shared_ptr;
using ::std::string;

constexpr char kGadgetName[] = "11210000.dwc3";
#define NEW_UDC_PATH "/sys/devices/platform/11210000.usb/"

#define ID_PATH NEW_UDC_PATH "dwc3_exynos_otg_id"
#define VBUS_PATH NEW_UDC_PATH "dwc3_exynos_otg_b_sess"
#define USB_DATA_PATH NEW_UDC_PATH "usb_data_enabled"

struct Usb : public BnUsb {
    Usb();

    ScopedAStatus enableContaminantPresenceDetection(const std::string& in_portName,
            bool in_enable, int64_t in_transactionId) override;
    ScopedAStatus queryPortStatus(int64_t in_transactionId) override;
    ScopedAStatus setCallback(const shared_ptr<IUsbCallback>& in_callback) override;
    ScopedAStatus switchRole(const string& in_portName, const PortRole& in_role,
            int64_t in_transactionId) override;
    ScopedAStatus enableUsbData(const string& in_portName, bool in_enable,
            int64_t in_transactionId) override;
    ScopedAStatus enableUsbDataWhileDocked(const string& in_portName,
            int64_t in_transactionId) override;
    ScopedAStatus limitPowerTransfer(const string& in_portName, bool in_limit,
            int64_t in_transactionId) override;
    ScopedAStatus resetUsbPort(const string& in_portName, int64_t in_transactionId) override;

    status_t handleShellCommand(int in, int out, int err, const char** argv,
            uint32_t argc) override;

    std::shared_ptr<::aidl::android::hardware::usb::IUsbCallback> mCallback;
    // Protects mCallback variable
    pthread_mutex_t mLock;
    // Protects roleSwitch operation
    pthread_mutex_t mRoleSwitchLock;
    // Threads waiting for the partner to come back wait here
    pthread_cond_t mPartnerCV;
    // lock protecting mPartnerCV
    pthread_mutex_t mPartnerLock;
    // Variable to signal partner coming back online after type switch
    bool mPartnerUp;

    // Report usb data session event and data incompliance warnings
    UsbDataSessionMonitor mUsbDataSessionMonitor;
    // Usb Overheat object for push suez event
    UsbOverheatEvent mOverheat;
    // Temperature when connected
    float mPluggedTemperatureCelsius;
    // Usb Data status
    bool mUsbDataEnabled;
    // Usb hub vendor command settings for JK level tuning
    int mUsbHubVendorCmdValue;
    int mUsbHubVendorCmdIndex;

  private:
    pthread_t mPoll;
    pthread_t mUsbHost;
};

using ext::PortSecurityState;

struct UsbExt : public ext::BnUsbExt {
    UsbExt(std::shared_ptr<Usb> usb);

    ScopedAStatus setPortSecurityState(const std::string& in_portName,
                                       PortSecurityState in_state) override;

    std::shared_ptr<Usb> mUsb;
};

} // namespace usb
} // namespace hardware
} // namespace android
} // aidl
