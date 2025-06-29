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

#define LOG_TAG "android.hardware.usb.aidl-service.UsbDataSessionMonitor"

#include "UsbDataSessionMonitor.h"

#include <aidl/android/frameworks/stats/IStats.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android_hardware_usb_flags.h>
#include <cutils/uevent.h>
#include <pixelstats/StatsHelper.h>
#include <pixelusb/CommonUtils.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <utils/Log.h>

#include <regex>

namespace usb_flags = android::hardware::usb::flags;

using aidl::android::frameworks::stats::IStats;
using android::base::ReadFileToString;
using android::hardware::google::pixel::getStatsService;
using android::hardware::google::pixel::reportUsbDataSessionEvent;
using android::hardware::google::pixel::PixelAtoms::VendorUsbDataSessionEvent;
using android::hardware::google::pixel::usb::addEpollFd;
using android::hardware::google::pixel::usb::BuildVendorUsbDataSessionEvent;

namespace aidl {
namespace android {
namespace hardware {
namespace usb {

#define UEVENT_MSG_LEN 2048
#define USB_STATE_MAX_LEN 20
#define DATA_ROLE_MAX_LEN 10
#define WARNING_SURFACE_DELAY_SEC 5
#define ENUM_FAIL_DEFAULT_COUNT_THRESHOLD 3
#define DEVICE_FLAKY_CONNECTION_CONFIGURED_COUNT_THRESHOLD 5

constexpr char kUdcConfigfsPath[] = "/config/usb_gadget/g1/UDC";
constexpr char kNotAttachedState[] = "not attached\n";
constexpr char kAttachedState[] = "attached\n";
constexpr char kPoweredState[] = "powered\n";
constexpr char kDefaultState[] = "default\n";
constexpr char kAddressedState[] = "addressed\n";
constexpr char kConfiguredState[] = "configured\n";
constexpr char kSuspendedState[] = "suspended\n";
const std::set<std::string> kValidStates = {kNotAttachedState, kAttachedState,  kPoweredState,
                                            kDefaultState,     kAddressedState, kConfiguredState,
                                            kSuspendedState};

static int addEpollFile(const int &epollFd, const std::string &filePath, unique_fd &fileFd) {
    struct epoll_event ev;

    unique_fd fd(open(filePath.c_str(), O_RDONLY));

    if (fd.get() == -1) {
        ALOGI("Cannot open %s", filePath.c_str());
        return -1;
    }

    ev.data.fd = fd.get();
    ev.events = EPOLLPRI;

    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd.get(), &ev) != 0) {
        ALOGE("epoll_ctl failed; errno=%d", errno);
        return -1;
    }

    fileFd = std::move(fd);
    ALOGI("epoll registered %s", filePath.c_str());
    return 0;
}

static void removeEpollFile(const int &epollFd, const std::string &filePath, unique_fd &fileFd) {
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fileFd.get(), NULL);
    fileFd.release();

    ALOGI("epoll unregistered %s", filePath.c_str());
}

UsbDataSessionMonitor::UsbDataSessionMonitor(
    const std::string &deviceUeventRegex, const std::string &deviceStatePath,
    const std::string &host1UeventRegex, const std::string &host1StatePath,
    const std::string &host2UeventRegex, const std::string &host2StatePath,
    const std::string &dataRolePath, std::function<void()> updatePortStatusCb) {
    struct epoll_event ev;
    std::string udc;

    unique_fd epollFd(epoll_create(8));
    if (epollFd.get() == -1) {
        ALOGE("epoll_create failed; errno=%d", errno);
        abort();
    }

    unique_fd ueventFd(uevent_open_socket(64 * 1024, true));
    if (ueventFd.get() == -1) {
        ALOGE("uevent_open_socket failed");
        abort();
    }
    fcntl(ueventFd, F_SETFL, O_NONBLOCK);

    if (addEpollFd(epollFd, ueventFd))
        abort();

    unique_fd timerFd(timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK));
    if (timerFd.get() == -1) {
        ALOGE("create timerFd failed");
        abort();
    }

    if (addEpollFd(epollFd, timerFd))
        abort();

    if (addEpollFile(epollFd.get(), dataRolePath, mDataRoleFd) != 0) {
        ALOGE("monitor data role failed");
        abort();
    }

    /*
     * The device state file could be absent depending on the current data role
     * and driver architecture. It's ok for addEpollFile to fail here, the file
     * will be monitored later when its presence is detected by uevent.
     */
    mDeviceState.filePath = deviceStatePath;
    mDeviceState.ueventRegex = deviceUeventRegex;
    addEpollFile(epollFd.get(), mDeviceState.filePath, mDeviceState.fd);

    mHost1State.filePath = host1StatePath;
    mHost1State.ueventRegex = host1UeventRegex;
    addEpollFile(epollFd.get(), mHost1State.filePath, mHost1State.fd);

    mHost2State.filePath = host2StatePath;
    mHost2State.ueventRegex = host2UeventRegex;
    addEpollFile(epollFd.get(), mHost2State.filePath, mHost2State.fd);

    mEpollFd = std::move(epollFd);
    mUeventFd = std::move(ueventFd);
    mTimerFd = std::move(timerFd);
    mUpdatePortStatusCb = updatePortStatusCb;

    if (ReadFileToString(kUdcConfigfsPath, &udc) && !udc.empty())
        mUdcBind = true;
    else
        mUdcBind = false;

    if (pthread_create(&mMonitor, NULL, this->monitorThread, this)) {
        ALOGE("pthread creation failed %d", errno);
        abort();
    }

    ALOGI("feature flag enable_report_usb_data_compliance_warning: %d",
          usb_flags::enable_report_usb_data_compliance_warning());
}

UsbDataSessionMonitor::~UsbDataSessionMonitor() {}

void UsbDataSessionMonitor::reportUsbDataSessionMetrics() {
    std::vector<VendorUsbDataSessionEvent> events;

    if (mDataRole == PortDataRole::DEVICE) {
        VendorUsbDataSessionEvent event;
        BuildVendorUsbDataSessionEvent(false /* is_host */, boot_clock::now(), mDataSessionStart,
                                       &mDeviceState.states, &mDeviceState.timestamps, &event);
        events.push_back(event);
    } else if (mDataRole == PortDataRole::HOST) {
        bool empty = true;
        for (auto e : {&mHost1State, &mHost2State}) {
            /*
             * Host port will at least get an not_attached event after enablement,
             * skip upload if no additional state is added.
             */
            if (e->states.size() > 1) {
                VendorUsbDataSessionEvent event;
                BuildVendorUsbDataSessionEvent(true /* is_host */, boot_clock::now(),
                                               mDataSessionStart, &e->states, &e->timestamps,
                                               &event);
                events.push_back(event);
                empty = false;
            }
        }
        // All host ports have no state update, upload an event to reflect it
        if (empty) {
            VendorUsbDataSessionEvent event;
            BuildVendorUsbDataSessionEvent(true /* is_host */, boot_clock::now(), mDataSessionStart,
                                           &mHost1State.states, &mHost1State.timestamps, &event);
            events.push_back(event);
        }
    } else {
        return;
    }

    const std::shared_ptr<IStats> stats_client = getStatsService();
    if (!stats_client) {
        ALOGE("Unable to get AIDL Stats service");
        return;
    }

    for (auto &event : events) {
        reportUsbDataSessionEvent(stats_client, event);
    }
}

void UsbDataSessionMonitor::getComplianceWarnings(const PortDataRole &role,
                                                  std::vector<ComplianceWarning> *warnings) {
    if (!usb_flags::enable_report_usb_data_compliance_warning())
        return;

    if (role != mDataRole || role == PortDataRole::NONE)
        return;

    for (auto w : mWarningSet) {
        warnings->push_back(w);
    }
}

void UsbDataSessionMonitor::notifyComplianceWarning() {
    if (!usb_flags::enable_report_usb_data_compliance_warning())
        return;

    if (mUpdatePortStatusCb)
        mUpdatePortStatusCb();
}

void UsbDataSessionMonitor::evaluateComplianceWarning() {
    std::set<ComplianceWarning> newWarningSet;
    int elapsedTimeSec;

    elapsedTimeSec =
        std::chrono::duration_cast<std::chrono::seconds>(boot_clock::now() - mDataSessionStart)
            .count();

    if (elapsedTimeSec >= WARNING_SURFACE_DELAY_SEC) {
        if (mDataRole == PortDataRole::DEVICE && mUdcBind) {
            int configuredCount = std::count(mDeviceState.states.begin(),
                                             mDeviceState.states.end(), kConfiguredState);
            int defaultCount =
                std::count(mDeviceState.states.begin(), mDeviceState.states.end(), kDefaultState);

            if (configuredCount == 0 && defaultCount > ENUM_FAIL_DEFAULT_COUNT_THRESHOLD)
                newWarningSet.insert(ComplianceWarning::ENUMERATION_FAIL);

            if (configuredCount > DEVICE_FLAKY_CONNECTION_CONFIGURED_COUNT_THRESHOLD)
                newWarningSet.insert(ComplianceWarning::FLAKY_CONNECTION);
        } else if (mDataRole == PortDataRole::HOST) {
            int host1StateCount = mHost1State.states.size();
            int host1ConfiguredCount =
                std::count(mHost1State.states.begin(), mHost1State.states.end(), kConfiguredState);
            int host1DefaultCount =
                std::count(mHost1State.states.begin(), mHost1State.states.end(), kDefaultState);
            int host2StateCount = mHost2State.states.size();
            int host2ConfiguredCount =
                std::count(mHost2State.states.begin(), mHost2State.states.end(), kConfiguredState);
            int host2DefaultCount =
                std::count(mHost2State.states.begin(), mHost2State.states.end(), kDefaultState);

            if (host1ConfiguredCount == 0 && host2ConfiguredCount == 0 &&
                (host1DefaultCount > ENUM_FAIL_DEFAULT_COUNT_THRESHOLD ||
                 host2DefaultCount > ENUM_FAIL_DEFAULT_COUNT_THRESHOLD))
                newWarningSet.insert(ComplianceWarning::ENUMERATION_FAIL);

            if (host1StateCount == 1 && mHost1State.states.front() == kNotAttachedState &&
                host2StateCount == 1 && mHost2State.states.front() == kNotAttachedState)
                newWarningSet.insert(ComplianceWarning::MISSING_DATA_LINES);
        }
    }

    if (newWarningSet != mWarningSet) {
        std::string newWarningString;

        for (auto e : newWarningSet) {
            newWarningString += toString(e) + " ";
        }
        ALOGI("Usb data compliance warning changed to: %s", newWarningString.c_str());

        mWarningSet = newWarningSet;
        notifyComplianceWarning();
    }
}

void UsbDataSessionMonitor::clearDeviceStateEvents(struct usbDeviceState *deviceState) {
    deviceState->states.clear();
    deviceState->timestamps.clear();
}

void UsbDataSessionMonitor::handleDeviceStateEvent(struct usbDeviceState *deviceState) {
    int n;
    char state[USB_STATE_MAX_LEN] = {0};

    lseek(deviceState->fd.get(), 0, SEEK_SET);
    n = read(deviceState->fd.get(), &state, USB_STATE_MAX_LEN);

    if (kValidStates.find(state) == kValidStates.end()) {
        ALOGE("Invalid state %s", state);
        return;
    }

    ALOGI("Update USB device state: %s", state);

    deviceState->states.push_back(state);
    deviceState->timestamps.push_back(boot_clock::now());
    evaluateComplianceWarning();
}

void UsbDataSessionMonitor::setupNewSession() {
    mWarningSet.clear();
    mDataSessionStart = boot_clock::now();

    if (mDataRole == PortDataRole::DEVICE) {
        clearDeviceStateEvents(&mDeviceState);
    } else if (mDataRole == PortDataRole::HOST) {
        clearDeviceStateEvents(&mHost1State);
        clearDeviceStateEvents(&mHost2State);
    }

    if (mDataRole != PortDataRole::NONE) {
        struct itimerspec delay = itimerspec();
        delay.it_value.tv_sec = WARNING_SURFACE_DELAY_SEC;
        int ret = timerfd_settime(mTimerFd.get(), 0, &delay, NULL);
        if (ret < 0)
            ALOGE("timerfd_settime failed err:%d", errno);
    }
}

void UsbDataSessionMonitor::handleDataRoleEvent() {
    int n;
    PortDataRole newDataRole;
    char role[DATA_ROLE_MAX_LEN] = {0};

    lseek(mDataRoleFd.get(), 0, SEEK_SET);
    n = read(mDataRoleFd.get(), &role, DATA_ROLE_MAX_LEN);

    ALOGI("Update USB data role %s", role);

    if (!std::strcmp(role, "host")) {
        newDataRole = PortDataRole::HOST;
    } else if (!std::strcmp(role, "device")) {
        newDataRole = PortDataRole::DEVICE;
    } else {
        newDataRole = PortDataRole::NONE;
    }

    if (newDataRole != mDataRole) {
        // Upload metrics for the last data session that has ended
        if (mDataRole == PortDataRole::HOST || (mDataRole == PortDataRole::DEVICE && mUdcBind)) {
            reportUsbDataSessionMetrics();
        }

        mDataRole = newDataRole;
        setupNewSession();
    }
}

void UsbDataSessionMonitor::updateUdcBindStatus(const std::string &devname) {
    std::string function;
    bool newUdcBind;

    /*
     * /sys/class/udc/<udc>/function prints out name of currently running USB gadget driver
     * Ref: https://www.kernel.org/doc/Documentation/ABI/stable/sysfs-class-udc
     * Empty name string means the udc device is not bound and gadget is pulldown.
     */
    if (!ReadFileToString("/sys" + devname + "/function", &function))
        return;

    if (function == "")
        newUdcBind = false;
    else
        newUdcBind = true;

    if (newUdcBind == mUdcBind)
        return;

    if (mDataRole == PortDataRole::DEVICE) {
        if (mUdcBind && !newUdcBind) {
            /*
             * Gadget soft pulldown: report metrics as the end of a data session and
             * re-evaluate compliance warnings to clear existing warnings if any.
             */
            reportUsbDataSessionMetrics();
            evaluateComplianceWarning();

        } else if (!mUdcBind && newUdcBind) {
            // Gadget soft pullup: reset and start accounting for a new data session.
            setupNewSession();
        }
    }

    ALOGI("Udc bind status changes from %b to %b", mUdcBind, newUdcBind);
    mUdcBind = newUdcBind;
}

void UsbDataSessionMonitor::handleUevent() {
    char msg[UEVENT_MSG_LEN + 2];
    char *cp;
    int n;

    n = uevent_kernel_multicast_recv(mUeventFd.get(), msg, UEVENT_MSG_LEN);
    if (n <= 0)
        return;
    if (n >= UEVENT_MSG_LEN)
        return;

    msg[n] = '\0';
    msg[n + 1] = '\0';
    cp = msg;

    while (*cp) {
        for (auto e : {&mHost1State, &mHost2State}) {
            if (std::regex_search(cp, std::regex(e->ueventRegex))) {
                if (!strncmp(cp, "bind@", strlen("bind@"))) {
                    addEpollFile(mEpollFd.get(), e->filePath, e->fd);
                } else if (!strncmp(cp, "unbind@", strlen("unbind@"))) {
                    removeEpollFile(mEpollFd.get(), e->filePath, e->fd);
                }
            }
        }

        // TODO: support bind@ unbind@ to detect dynamically allocated udc device
        if (std::regex_search(cp, std::regex(mDeviceState.ueventRegex))) {
            if (!strncmp(cp, "change@", strlen("change@"))) {
                char *devname = cp + strlen("change@");
                /*
                 * Udc device emits a KOBJ_CHANGE event on configfs driver bind and unbind.
                 * TODO: upstream udc driver emits KOBJ_CHANGE event BEFORE unbind is actually
                 * executed. Add a short delay to get the correct state while working on a fix
                 * upstream.
                 */
                usleep(50000);
                updateUdcBindStatus(devname);
            }
        }
        /* advance to after the next \0 */
        while (*cp++) {
        }
    }
}

void UsbDataSessionMonitor::handleTimerEvent() {
    int byteRead;
    uint64_t numExpiration;

    byteRead = read(mTimerFd.get(), &numExpiration, sizeof(numExpiration));

    if (byteRead != sizeof(numExpiration)) {
        ALOGE("incorrect read size");
    }

    if (numExpiration != 1) {
        ALOGE("incorrect expiration count");
    }

    evaluateComplianceWarning();
}

void *UsbDataSessionMonitor::monitorThread(void *param) {
    UsbDataSessionMonitor *monitor = (UsbDataSessionMonitor *)param;
    struct epoll_event events[64];
    int nevents = 0;

    while (true) {
        nevents = epoll_wait(monitor->mEpollFd.get(), events, 64, -1);
        if (nevents == -1) {
            if (errno == EINTR)
                continue;
            ALOGE("usb epoll_wait failed; errno=%d", errno);
            break;
        }

        for (int n = 0; n < nevents; ++n) {
            if (events[n].data.fd == monitor->mUeventFd.get()) {
                monitor->handleUevent();
            } else if (events[n].data.fd == monitor->mTimerFd.get()) {
                monitor->handleTimerEvent();
            } else if (events[n].data.fd == monitor->mDataRoleFd.get()) {
                monitor->handleDataRoleEvent();
            } else if (events[n].data.fd == monitor->mDeviceState.fd.get()) {
                monitor->handleDeviceStateEvent(&monitor->mDeviceState);
            } else if (events[n].data.fd == monitor->mHost1State.fd.get()) {
                monitor->handleDeviceStateEvent(&monitor->mHost1State);
            } else if (events[n].data.fd == monitor->mHost2State.fd.get()) {
                monitor->handleDeviceStateEvent(&monitor->mHost2State);
            }
        }
    }
    return NULL;
}

}  // namespace usb
}  // namespace hardware
}  // namespace android
}  // namespace aidl
