/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <aidl/android/hardware/dumpstate/BnDumpstateDevice.h>
#include <aidl/android/hardware/dumpstate/IDumpstateDevice.h>
#include <android/binder_status.h>

namespace aidl {
namespace android {
namespace hardware {
namespace dumpstate {

class Dumpstate : public BnDumpstateDevice {
  public:
    Dumpstate();

    ::ndk::ScopedAStatus dumpstateBoard(const std::vector<::ndk::ScopedFileDescriptor>& in_fds,
                                        IDumpstateDevice::DumpstateMode in_mode,
                                        int64_t in_timeoutMillis) override;

    ::ndk::ScopedAStatus getVerboseLoggingEnabled(bool* _aidl_return) override;

    ::ndk::ScopedAStatus setVerboseLoggingEnabled(bool in_enable) override;

    binder_status_t dump(int fd, const char** args, uint32_t numArgs) override;

  private:
    const std::string kAllSections = "all";

    std::vector<std::pair<std::string, std::function<void(int)>>> mTextSections;
    std::vector<std::pair<std::string, std::function<void(int, const std::string &)>>> mLogSections;

    void dumpLogs(int fd, std::string srcDir, std::string destDir, int maxFileNum,
                  const char *logPrefix);

    void dumpTextSection(int fd, std::string const& sectionName);

    // Text sections that can be dumped individually on the command line in
    // addition to being included in full dumps
    void dumpWlanSection(int fd);
    void dumpPowerSection(int fd);
    void dumpThermalSection(int fd);
    void dumpTouchSection(int fd);
    void dumpCpuSection(int fd);
    void dumpDevfreqSection(int fd);
    void dumpMemorySection(int fd);
    void dumpDisplaySection(int fd);
    void dumpMiscSection(int fd);
    void dumpLEDSection(int fd);

    void dumpLogSection(int fd, int fdModem);

    // Log sections to be dumped individually into dumpstate_board.bin
    void dumpModemLogs(int fd, const std::string &destDir);
    void dumpRadioLogs(int fd, const std::string &destDir);
    void dumpCameraLogs(int fd, const std::string &destDir);
    void dumpGpsLogs(int fd, const std::string &destDir);
    void dumpGxpLogs(int fd, const std::string &destDir);

    // Hybrid and binary sections that require an additional file descriptor
    void dumpRilLogs(int fd, std::string destDir);

    //bool getVerboseLoggingEnabledImpl();
    //::ndk::ScopedAStatus dumpstateBoardImpl(const int fd, const bool full);
};

}  // namespace dumpstate
}  // namespace hardware
}  // namespace android
}  // namespace aidl
