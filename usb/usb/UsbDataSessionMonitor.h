/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <aidl/android/hardware/usb/ComplianceWarning.h>
#include <aidl/android/hardware/usb/PortDataRole.h>
#include <android-base/chrono_utils.h>
#include <android-base/unique_fd.h>

#include <set>
#include <string>
#include <vector>

namespace aidl {
namespace android {
namespace hardware {
namespace usb {

using ::aidl::android::hardware::usb::ComplianceWarning;
using ::aidl::android::hardware::usb::PortDataRole;
using ::android::base::boot_clock;
using ::android::base::unique_fd;

/*
 * UsbDataSessionMonitor monitors the usb device state sysfs of 3 different usb devices
 * including device mode (udc), host mode high-speed port and host mode super-speed port. It
 * reports Suez metrics for each data session and also provides API to query the compliance
 * warnings detected in the current usb data session.
 */
class UsbDataSessionMonitor {
  public:
    /*
     * The host mode high-speed port and super-speed port can be assigned to either host1 or
     * host2 without affecting functionality.
     *
     * UeventRegex: name regex of the device that's being monitored. The regex is matched against
     *              uevent to detect dynamic creation/deletion/change of the device.
     * StatePath: usb device state sysfs path of the device, monitored by epoll.
     * dataRolePath: path to the usb data role sysfs, monitored by epoll.
     * updatePortStatusCb: the callback is invoked when the compliance warings changes.
     */
    UsbDataSessionMonitor(const std::string &deviceUeventRegex, const std::string &deviceStatePath,
                          const std::string &host1UeventRegex, const std::string &host1StatePath,
                          const std::string &host2UeventRegex, const std::string &host2StatePath,
                          const std::string &dataRolePath,
                          std::function<void()> updatePortStatusCb);
    ~UsbDataSessionMonitor();
    // Returns the compliance warnings detected in the current data session.
    void getComplianceWarnings(const PortDataRole &role, std::vector<ComplianceWarning> *warnings);

  private:
    struct usbDeviceState {
        unique_fd fd;
        std::string filePath;
        std::string ueventRegex;
        // Usb device states reported by state sysfs
        std::vector<std::string> states;
        // Timestamps of when the usb device states were captured
        std::vector<boot_clock::time_point> timestamps;
    };

    static void *monitorThread(void *param);
    void handleUevent();
    void handleDataRoleEvent();
    void handleDeviceStateEvent(struct usbDeviceState *deviceState);
    void clearDeviceStateEvents(struct usbDeviceState *deviceState);
    void reportUsbDataSessionMetrics();
    void evaluateComplianceWarning();
    void notifyComplianceWarning();
    void updateUdcBindStatus(const std::string &devname);

    pthread_t mMonitor;
    unique_fd mEpollFd;
    unique_fd mUeventFd;
    unique_fd mDataRoleFd;
    struct usbDeviceState mDeviceState;
    struct usbDeviceState mHost1State;
    struct usbDeviceState mHost2State;
    std::set<ComplianceWarning> mWarningSet;
    // Callback function to notify the caller when there's a change in compliance warnings.
    std::function<void()> mUpdatePortStatusCb;
    /*
     * Cache relevant info for a USB data session when one starts, including
     * the data role and the time when the session starts.
     */
    PortDataRole mDataRole;
    boot_clock::time_point mDataSessionStart;
    /*
     * In gadget mode: this indicates whether the udc device is bound to the configfs driver, which
     * is done by userspace writing the udc device name to /config/usb_gadget/g1/UDC. When unbound,
     * the gadget is in soft pulldown state and is expected not to enumerate. During gadget
     * function switch, the udc device usually go through unbind and bind.
     */
    bool mUdcBind;
};

}  // namespace usb
}  // namespace hardware
}  // namespace android
}  // namespace aidl
