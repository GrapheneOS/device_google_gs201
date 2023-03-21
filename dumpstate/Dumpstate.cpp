/*
 * Copyright 2016 The Android Open Source Project
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

#define LOG_TAG "dumpstate_device"
#define ATRACE_TAG ATRACE_TAG_ALWAYS

#include <inttypes.h>

#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include <android-base/properties.h>
#include <android-base/unique_fd.h>
#include <cutils/trace.h>
#include <log/log.h>
#include <sys/stat.h>
#include <dump/pixel_dump.h>

#include "Dumpstate.h"

#include "DumpstateUtil.h"

#define RIL_LOG_DIRECTORY "/data/vendor/radio"
#define RIL_LOG_DIRECTORY_PROPERTY "persist.vendor.ril.log.base_dir"
#define RIL_LOG_NUMBER_PROPERTY "persist.vendor.ril.log.num_file"
#define GPS_LOG_DIRECTORY "/data/vendor/gps/logs"
#define GPS_LOG_NUMBER_PROPERTY "persist.vendor.gps.aol.log_num"
#define GPS_LOGGING_STATUS_PROPERTY "vendor.gps.aol.enabled"

#define TCPDUMP_LOG_DIRECTORY "/data/vendor/tcpdump_logger/logs"
#define TCPDUMP_NUMBER_BUGREPORT "persist.vendor.tcpdump.log.br_num"
#define TCPDUMP_PERSIST_PROPERTY "persist.vendor.tcpdump.log.alwayson"

using android::os::dumpstate::CommandOptions;
using android::os::dumpstate::DumpFileToFd;
using android::os::dumpstate::PropertiesHelper;
using android::os::dumpstate::RunCommandToFd;

namespace aidl {
namespace android {
namespace hardware {
namespace dumpstate {

#define GPS_LOG_PREFIX "gl-"
#define GPS_MCU_LOG_PREFIX "esw-"
#define EXTENDED_LOG_PREFIX "extended_log_"
#define RIL_LOG_PREFIX "rild.log."
#define BUFSIZE 65536
#define TCPDUMP_LOG_PREFIX "tcpdump"

typedef std::chrono::time_point<std::chrono::steady_clock> timepoint_t;

const char kVerboseLoggingProperty[] = "persist.vendor.verbose_logging_enabled";

void Dumpstate::dumpLogs(int fd, std::string srcDir, std::string destDir, int maxFileNum,
                               const char *logPrefix) {
    struct dirent **dirent_list = NULL;
    int num_entries = scandir(srcDir.c_str(),
                              &dirent_list,
                              0,
                              (int (*)(const struct dirent **, const struct dirent **)) alphasort);
    if (!dirent_list) {
        return;
    } else if (num_entries <= 0) {
        return;
    }

    int copiedFiles = 0;

    for (int i = num_entries - 1; i >= 0; i--) {
        ALOGD("Found %s\n", dirent_list[i]->d_name);

        if (0 != strncmp(dirent_list[i]->d_name, logPrefix, strlen(logPrefix))) {
            continue;
        }

        if ((copiedFiles >= maxFileNum) && (maxFileNum != -1)) {
            ALOGD("Skipped %s\n", dirent_list[i]->d_name);
            continue;
        }

        copiedFiles++;

        CommandOptions options = CommandOptions::WithTimeout(120).Build();
        std::string srcLogFile = srcDir + "/" + dirent_list[i]->d_name;
        std::string destLogFile = destDir + "/" + dirent_list[i]->d_name;

        std::string copyCmd = "/vendor/bin/cp " + srcLogFile + " " + destLogFile;

        ALOGD("Copying %s to %s\n", srcLogFile.c_str(), destLogFile.c_str());
        RunCommandToFd(fd, "CP LOGS", { "/vendor/bin/sh", "-c", copyCmd.c_str() }, options);
    }

    while (num_entries--) {
        free(dirent_list[num_entries]);
    }

    free(dirent_list);
}

void Dumpstate::dumpRilLogs(int fd, std::string destDir) {
    std::string rilLogDir =
            ::android::base::GetProperty(RIL_LOG_DIRECTORY_PROPERTY, RIL_LOG_DIRECTORY);

    int maxFileNum = ::android::base::GetIntProperty(RIL_LOG_NUMBER_PROPERTY, 50);

    const std::string currentLogDir = rilLogDir + "/cur";
    const std::string previousLogDir = rilLogDir + "/prev";
    const std::string currentDestDir = destDir + "/cur";
    const std::string previousDestDir = destDir + "/prev";

    RunCommandToFd(fd, "MKDIR RIL CUR LOG", {"/vendor/bin/mkdir", "-p", currentDestDir.c_str()},
                   CommandOptions::WithTimeout(2).Build());
    RunCommandToFd(fd, "MKDIR RIL PREV LOG", {"/vendor/bin/mkdir", "-p", previousDestDir.c_str()},
                   CommandOptions::WithTimeout(2).Build());

    dumpLogs(fd, currentLogDir, currentDestDir, maxFileNum, RIL_LOG_PREFIX);
    dumpLogs(fd, previousLogDir, previousDestDir, maxFileNum, RIL_LOG_PREFIX);
}

void copyFile(std::string srcFile, std::string destFile) {
    uint8_t buffer[BUFSIZE];
    ssize_t size;

    int fdSrc = open(srcFile.c_str(), O_RDONLY);
    if (fdSrc < 0) {
        ALOGD("Failed to open source file %s\n", srcFile.c_str());
        return;
    }

    int fdDest = open(destFile.c_str(), O_WRONLY | O_CREAT, 0666);
    if (fdDest < 0) {
        ALOGD("Failed to open destination file %s\n", destFile.c_str());
        close(fdSrc);
        return;
    }

    ALOGD("Copying %s to %s\n", srcFile.c_str(), destFile.c_str());
    while ((size = TEMP_FAILURE_RETRY(read(fdSrc, buffer, BUFSIZE))) > 0) {
        TEMP_FAILURE_RETRY(write(fdDest, buffer, size));
    }

    close(fdDest);
    close(fdSrc);
}

void dumpNetmgrLogs(std::string destDir) {
    const std::vector <std::string> netmgrLogs
        {
            "/data/vendor/radio/metrics_data",
            "/data/vendor/radio/omadm_logs.txt",
            "/data/vendor/radio/power_anomaly_data.txt",
        };
    for (const auto& logFile : netmgrLogs) {
        copyFile(logFile, destDir + "/" + basename(logFile.c_str()));
    }
}

timepoint_t startSection(int fd, const std::string &sectionName) {
    ATRACE_BEGIN(sectionName.c_str());
    ::android::base::WriteStringToFd(
            "\n"
            "------ Section start: " + sectionName + " ------\n"
            "\n", fd);
    return std::chrono::steady_clock::now();
}

void endSection(int fd, const std::string &sectionName, timepoint_t startTime) {
    ATRACE_END();
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMsec = std::chrono::duration_cast<std::chrono::milliseconds>
            (endTime - startTime).count();

    ::android::base::WriteStringToFd(
            "\n"
            "------ Section end: " + sectionName + " ------\n"
            "Elapsed msec: " + std::to_string(elapsedMsec) + "\n"
            "\n", fd);
}

Dumpstate::Dumpstate()
  : mTextSections{
        { "wlan", [this](int fd) { dumpWlanSection(fd); } },
        { "power", [this](int fd) { dumpPowerSection(fd); } },
        { "pixel-trace", [this](int fd) { dumpPixelTraceSection(fd); } },
    },
  mLogSections{
        { "radio", [this](int fd, const std::string &destDir) { dumpRadioLogs(fd, destDir); } },
        { "gps", [this](int fd, const std::string &destDir) { dumpGpsLogs(fd, destDir); } },
        { "gxp", [this](int fd, const std::string &destDir) { dumpGxpLogs(fd, destDir); } },
  } {
}

// Dump data requested by an argument to the "dump" interface, or help info
// if the specified section is not supported.
void Dumpstate::dumpTextSection(int fd, const std::string &sectionName) {
    bool dumpAll = (sectionName == kAllSections);
    std::string dumpFiles;

    for (const auto &section : mTextSections) {
        if (dumpAll || sectionName == section.first) {
            auto startTime = startSection(fd, section.first);
            section.second(fd);
            endSection(fd, section.first, startTime);

            if (!dumpAll) {
                return;
            }
        }
    }

    // Execute all or designated programs under vendor/bin/dump/
    std::unique_ptr<DIR, decltype(&closedir)> dir(opendir("/vendor/bin/dump"), closedir);
    if (!dir) {
        ALOGE("Fail To Open Dir vendor/bin/dump/");
        ::android::base::WriteStringToFd("Fail To Open Dir vendor/bin/dump/\n", fd);
        return;
    }
    dirent *entry;
    while ((entry = readdir(dir.get())) != nullptr) {
        // Skip '.', '..'
        if (entry->d_name[0] == '.') {
            continue;
        }
        std::string bin(entry->d_name);
        dumpFiles = dumpFiles + " " + bin;
        if (dumpAll || sectionName == bin) {
            auto startTime = startSection(fd, bin);
            RunCommandToFd(fd, "/vendor/bin/dump/"+bin, {"/vendor/bin/dump/"+bin}, CommandOptions::WithTimeout(15).Build());
            endSection(fd, bin, startTime);
            if (!dumpAll) {
                return;
            }
        }
    }

    if (dumpAll) {
        RunCommandToFd(fd, "VENDOR PROPERTIES", {"/vendor/bin/getprop"});
        return;
    }

    // An unsupported section was requested on the command line
    ::android::base::WriteStringToFd("Unrecognized text section: " + sectionName + "\n", fd);
    ::android::base::WriteStringToFd("Try \"" + kAllSections + "\" or one of the following:", fd);
    for (const auto &section : mTextSections) {
        ::android::base::WriteStringToFd(" " + section.first, fd);
    }
    ::android::base::WriteStringToFd(dumpFiles, fd);
    ::android::base::WriteStringToFd("\nNote: sections with attachments (e.g. modem) are"
                                   "not avalable from the command line.\n", fd);
}

// Dump items related to wlan
void Dumpstate::dumpWlanSection(int fd) {
    // Dump firmware symbol table for firmware log decryption
    DumpFileToFd(fd, "WLAN FW Log Symbol Table", "/vendor/firmware/Data.msc");
    RunCommandToFd(fd, "WLAN TWT Dump", {"/vendor/bin/sh", "-c",
                    "cat /sys/wlan_ptracker/twt/*"});
}

// Dump items related to power and battery
void Dumpstate::dumpPowerSection(int fd) {
    struct stat buffer;

    RunCommandToFd(fd, "Power Stats Times", {"/vendor/bin/sh", "-c",
                   "echo -n \"Boot: \" && /vendor/bin/uptime -s && "
                   "echo -n \"Now: \" && date"});

    RunCommandToFd(fd, "ACPM stats", {"/vendor/bin/sh", "-c",
                   "for f in /sys/devices/platform/acpm_stats/*_stats ; do "
                   "echo \"\\n\\n$f\" ; cat $f ; "
                   "done"});

    DumpFileToFd(fd, "CPU PM stats", "/sys/devices/system/cpu/cpupm/cpupm/time_in_state");

    DumpFileToFd(fd, "GENPD summary", "/d/pm_genpd/pm_genpd_summary");

    DumpFileToFd(fd, "Power supply property battery", "/sys/class/power_supply/battery/uevent");
    DumpFileToFd(fd, "Power supply property dc", "/sys/class/power_supply/dc/uevent");
    DumpFileToFd(fd, "Power supply property gcpm", "/sys/class/power_supply/gcpm/uevent");
    DumpFileToFd(fd, "Power supply property gcpm_pps", "/sys/class/power_supply/gcpm_pps/uevent");
    DumpFileToFd(fd, "Power supply property main-charger", "/sys/class/power_supply/main-charger/uevent");
    if (!stat("/sys/class/power_supply/pca9468-mains/uevent", &buffer)) {
        DumpFileToFd(fd, "Power supply property pca9468-mains", "/sys/class/power_supply/pca9468-mains/uevent");
    } else {
        DumpFileToFd(fd, "Power supply property pca94xx-mains", "/sys/class/power_supply/pca94xx-mains/uevent");
    }
    DumpFileToFd(fd, "Power supply property tcpm", "/sys/class/power_supply/tcpm-source-psy-i2c-max77759tcpc/uevent");
    DumpFileToFd(fd, "Power supply property usb", "/sys/class/power_supply/usb/uevent");
    DumpFileToFd(fd, "Power supply property wireless", "/sys/class/power_supply/wireless/uevent");
    if (!stat("/sys/class/power_supply/maxfg", &buffer)) {
        DumpFileToFd(fd, "Power supply property maxfg", "/sys/class/power_supply/maxfg/uevent");
        DumpFileToFd(fd, "m5_state", "/sys/class/power_supply/maxfg/m5_model_state");
        DumpFileToFd(fd, "maxfg", "/dev/logbuffer_maxfg");
        DumpFileToFd(fd, "maxfg", "/dev/logbuffer_maxfg_monitor");
    } else {
        DumpFileToFd(fd, "Power supply property maxfg_base", "/sys/class/power_supply/maxfg_base/uevent");
        DumpFileToFd(fd, "Power supply property maxfg_secondary", "/sys/class/power_supply/maxfg_secondary/uevent");
        DumpFileToFd(fd, "m5_state", "/sys/class/power_supply/maxfg_base/m5_model_state");
        DumpFileToFd(fd, "maxfg_base", "/dev/logbuffer_maxfg_base");
        DumpFileToFd(fd, "maxfg_secondary", "/dev/logbuffer_maxfg_secondary");
        DumpFileToFd(fd, "maxfg_base", "/dev/logbuffer_maxfg_base_monitor");
        DumpFileToFd(fd, "maxfg_secondary", "/dev/logbuffer_maxfg_secondary_monitor");
        DumpFileToFd(fd, "google_dual_batt", "/dev/logbuffer_dual_batt");
    }

    if (!stat("/dev/maxfg_history", &buffer)) {
        DumpFileToFd(fd, "Maxim FG History", "/dev/maxfg_history");
    }

    if (!stat("/sys/class/power_supply/dock", &buffer)) {
        DumpFileToFd(fd, "Power supply property dock", "/sys/class/power_supply/dock/uevent");
    }

    if (!stat("/dev/logbuffer_tcpm", &buffer)) {
        DumpFileToFd(fd, "Logbuffer TCPM", "/dev/logbuffer_tcpm");
    } else if (!PropertiesHelper::IsUserBuild()) {
        if (!stat("/sys/kernel/debug/tcpm", &buffer)) {
            RunCommandToFd(fd, "TCPM logs", {"/vendor/bin/sh", "-c", "cat /sys/kernel/debug/tcpm/*"});
        } else {
            RunCommandToFd(fd, "TCPM logs", {"/vendor/bin/sh", "-c", "cat /sys/kernel/debug/usb/tcpm*"});
        }
    }

    RunCommandToFd(fd, "TCPC", {"/vendor/bin/sh", "-c",
		       "for f in /sys/devices/platform/10d60000.hsi2c/i2c-*/i2c-max77759tcpc;"
		       "do echo \"registers:\"; cat $f/registers;"
		       "echo \"frs:\"; cat $f/frs;"
		       "echo \"auto_discharge:\"; cat $f/auto_discharge;"
		       "echo \"bc12_enabled:\"; cat $f/bc12_enabled;"
		       "echo \"cc_toggle_enable:\"; cat $f/cc_toggle_enable;"
		       "echo \"contaminant_detection:\"; cat $f/contaminant_detection;"
		       "echo \"contaminant_detection_status:\"; cat $f/contaminant_detection_status;  done"});

    DumpFileToFd(fd, "PD Engine", "/dev/logbuffer_usbpd");
    DumpFileToFd(fd, "POGO Transport", "/dev/logbuffer_pogo_transport");
    DumpFileToFd(fd, "PPS-google_cpm", "/dev/logbuffer_cpm");
    DumpFileToFd(fd, "PPS-dc", "/dev/logbuffer_pca9468");

    DumpFileToFd(fd, "Battery Health", "/sys/class/power_supply/battery/health_index_stats");
    DumpFileToFd(fd, "BMS", "/dev/logbuffer_ssoc");
    DumpFileToFd(fd, "TTF", "/dev/logbuffer_ttf");
    DumpFileToFd(fd, "TTF details", "/sys/class/power_supply/battery/ttf_details");
    DumpFileToFd(fd, "TTF stats", "/sys/class/power_supply/battery/ttf_stats");
    DumpFileToFd(fd, "aacr_state", "/sys/class/power_supply/battery/aacr_state");
    DumpFileToFd(fd, "maxq", "/dev/logbuffer_maxq");
    DumpFileToFd(fd, "TEMP/DOCK-DEFEND", "/dev/logbuffer_bd");

    RunCommandToFd(fd, "TRICKLE-DEFEND Config", {"/vendor/bin/sh", "-c",
                        " cd /sys/devices/platform/google,battery/power_supply/battery/;"
                        " for f in `ls bd_*` ; do echo \"$f: `cat $f`\" ; done"});

    RunCommandToFd(fd, "DWELL-DEFEND Config", {"/vendor/bin/sh", "-c",
                        " cd /sys/devices/platform/google,charger/;"
                        " for f in `ls charge_s*` ; do echo \"$f: `cat $f`\" ; done"});

    RunCommandToFd(fd, "TEMP-DEFEND Config", {"/vendor/bin/sh", "-c",
                        " cd /sys/devices/platform/google,charger/;"
                        " for f in `ls bd_*` ; do echo \"$f: `cat $f`\" ; done"});

    if (!PropertiesHelper::IsUserBuild()) {
        DumpFileToFd(fd, "DC_registers dump", "/sys/class/power_supply/pca94xx-mains/device/registers_dump");
        DumpFileToFd(fd, "max77759_chg registers dump", "/d/max77759_chg/registers");
        DumpFileToFd(fd, "max77729_pmic registers dump", "/d/max77729_pmic/registers");
        DumpFileToFd(fd, "Charging table dump", "/d/google_battery/chg_raw_profile");

        RunCommandToFd(fd, "fg_model", {"/vendor/bin/sh", "-c",
                            "for f in /d/maxfg* ; do "
                            "regs=`cat $f/fg_model`; echo $f: ;"
                            "echo \"$regs\"; done"});

        RunCommandToFd(fd, "fg_alo_ver", {"/vendor/bin/sh", "-c",
                            "for f in /d/maxfg* ; do "
                            "regs=`cat $f/algo_ver`; echo $f: ;"
                            "echo \"$regs\"; done"});

        RunCommandToFd(fd, "fg_model_ok", {"/vendor/bin/sh", "-c",
                            "for f in /d/maxfg* ; do "
                            "regs=`cat $f/model_ok`; echo $f: ;"
                            "echo \"$regs\"; done"});

        /* FG Registers */
        RunCommandToFd(fd, "fg registers", {"/vendor/bin/sh", "-c",
                            "for f in /d/maxfg* ; do "
                            "regs=`cat $f/registers`; echo $f: ;"
                            "echo \"$regs\"; done"});

        RunCommandToFd(fd, "Maxim FG NV RAM", {"/vendor/bin/sh", "-c",
                            "for f in /d/maxfg* ; do "
                            "regs=`cat $f/nv_registers`; echo $f: ;"
                            "echo \"$regs\"; done"});
    }

    /* EEPROM State */
    if (!stat("/sys/devices/platform/10970000.hsi2c/i2c-4/4-0050/eeprom", &buffer)) {
        RunCommandToFd(fd, "Battery EEPROM", {"/vendor/bin/sh", "-c", "xxd /sys/devices/platform/10970000.hsi2c/i2c-4/4-0050/eeprom"});
    } else if (!stat("/sys/devices/platform/10970000.hsi2c/i2c-5/5-0050/eeprom", &buffer)) {
        RunCommandToFd(fd, "Battery EEPROM", {"/vendor/bin/sh", "-c", "xxd /sys/devices/platform/10970000.hsi2c/i2c-5/5-0050/eeprom"});
    } else if (!stat("/sys/devices/platform/10da0000.hsi2c/i2c-5/5-0050/eeprom", &buffer)) {
        RunCommandToFd(fd, "Battery EEPROM", {"/vendor/bin/sh", "-c", "xxd /sys/devices/platform/10da0000.hsi2c/i2c-5/5-0050/eeprom"});
    } else if (!stat("/sys/devices/platform/10da0000.hsi2c/i2c-6/6-0050/eeprom", &buffer)) {
        RunCommandToFd(fd, "Battery EEPROM", {"/vendor/bin/sh", "-c", "xxd /sys/devices/platform/10da0000.hsi2c/i2c-6/6-0050/eeprom"});
    } else if (!stat("/sys/devices/platform/10da0000.hsi2c/i2c-7/7-0050/eeprom", &buffer)) {
        RunCommandToFd(fd, "Battery EEPROM", {"/vendor/bin/sh", "-c", "xxd /sys/devices/platform/10da0000.hsi2c/i2c-7/7-0050/eeprom"});
    }

    DumpFileToFd(fd, "Charger Stats", "/sys/class/power_supply/battery/charge_details");
    if (!PropertiesHelper::IsUserBuild()) {
        RunCommandToFd(fd, "Google Charger", {"/vendor/bin/sh", "-c", "cd /sys/kernel/debug/google_charger/; "
                            "for f in `ls pps_*` ; do echo \"$f: `cat $f`\" ; done"});
        RunCommandToFd(fd, "Google Battery", {"/vendor/bin/sh", "-c", "cd /sys/kernel/debug/google_battery/; "
                            "for f in `ls ssoc_*` ; do echo \"$f: `cat $f`\" ; done"});
    }

    DumpFileToFd(fd, "WLC logs", "/dev/logbuffer_wireless");
    DumpFileToFd(fd, "WLC VER", "/sys/class/power_supply/wireless/device/version");
    DumpFileToFd(fd, "WLC STATUS", "/sys/class/power_supply/wireless/device/status");
    DumpFileToFd(fd, "WLC FW Version", "/sys/class/power_supply/wireless/device/fw_rev");
    DumpFileToFd(fd, "RTX", "/dev/logbuffer_rtx");

    if (!PropertiesHelper::IsUserBuild()) {
        RunCommandToFd(fd, "gvotables", {"/vendor/bin/sh", "-c", "cat /sys/kernel/debug/gvotables/*/status"});
    }
    DumpFileToFd(fd, "Lastmeal", "/data/vendor/mitigation/lastmeal.txt");
    DumpFileToFd(fd, "Thismeal", "/data/vendor/mitigation/thismeal.txt");
    RunCommandToFd(fd, "Mitigation Stats", {"/vendor/bin/sh", "-c", "echo \"Source\\t\\tCount\\tSOC\\tTime\\tVoltage\"; "
                        "for f in `ls /sys/devices/virtual/pmic/mitigation/last_triggered_count/*` ; "
                        "do count=`cat $f`; "
                        "a=${f/\\/sys\\/devices\\/virtual\\/pmic\\/mitigation\\/last_triggered_count\\//}; "
                        "b=${f/last_triggered_count/last_triggered_capacity}; "
                        "c=${f/last_triggered_count/last_triggered_timestamp/}; "
                        "d=${f/last_triggered_count/last_triggered_voltage/}; "
                        "cnt=`cat $f`; "
                        "cap=`cat ${b/count/cap}`; "
                        "ti=`cat ${c/count/time}`; "
                        "volt=`cat ${d/count/volt}`; "
                        "echo \"${a/_count/} "
                        "\\t$cnt\\t$cap\\t$ti\\t$volt\" ; done"});
    RunCommandToFd(fd, "Clock Divider Ratio", {"/vendor/bin/sh", "-c", "echo \"Source\\t\\tRatio\"; "
                        "for f in `ls /sys/devices/virtual/pmic/mitigation/clock_ratio/*` ; "
                        "do ratio=`cat $f`; "
                        "a=${f/\\/sys\\/devices\\/virtual\\/pmic\\/mitigation\\/clock_ratio\\//}; "
                        "echo \"${a/_ratio/} \\t$ratio\" ; done"});
    RunCommandToFd(fd, "Clock Stats", {"/vendor/bin/sh", "-c", "echo \"Source\\t\\tStats\"; "
                        "for f in `ls /sys/devices/virtual/pmic/mitigation/clock_stats/*` ; "
                        "do stats=`cat $f`; "
                        "a=${f/\\/sys\\/devices\\/virtual\\/pmic\\/mitigation\\/clock_stats\\//}; "
                        "echo \"${a/_stats/} \\t$stats\" ; done"});
    RunCommandToFd(fd, "Triggered Level", {"/vendor/bin/sh", "-c", "echo \"Source\\t\\tLevel\"; "
                        "for f in `ls /sys/devices/virtual/pmic/mitigation/triggered_lvl/*` ; "
                        "do lvl=`cat $f`; "
                        "a=${f/\\/sys\\/devices\\/virtual\\/pmic\\/mitigation\\/triggered_lvl\\//}; "
                        "echo \"${a/_lvl/} \\t$lvl\" ; done"});
    RunCommandToFd(fd, "Instruction", {"/vendor/bin/sh", "-c",
                        "for f in `ls /sys/devices/virtual/pmic/mitigation/instruction/*` ; "
                        "do val=`cat $f` ; "
                        "a=${f/\\/sys\\/devices\\/virtual\\/pmic\\/mitigation\\/instruction\\//}; "
                        "echo \"$a=$val\" ; done"});

}

void Dumpstate::dumpRadioLogs(int fd, const std::string &destDir) {
    std::string tcpdumpLogDir = TCPDUMP_LOG_DIRECTORY;
    bool tcpdumpEnabled = ::android::base::GetBoolProperty(TCPDUMP_PERSIST_PROPERTY, false);

    if (tcpdumpEnabled) {
        dumpLogs(fd, tcpdumpLogDir, destDir, ::android::base::GetIntProperty(TCPDUMP_NUMBER_BUGREPORT, 5), TCPDUMP_LOG_PREFIX);
    }
    dumpRilLogs(fd, destDir);
    dumpNetmgrLogs(destDir);
}

void Dumpstate::dumpGpsLogs(int fd, const std::string &destDir) {
    bool gpsLogEnabled = ::android::base::GetBoolProperty(GPS_LOGGING_STATUS_PROPERTY, false);
    if (!gpsLogEnabled) {
        ALOGD("gps logging is not running\n");
        return;
    }
    const std::string gpsLogDir = GPS_LOG_DIRECTORY;
    const std::string gpsTmpLogDir = gpsLogDir + "/.tmp";
    const std::string gpsDestDir = destDir + "/gps";

    int maxFileNum = ::android::base::GetIntProperty(GPS_LOG_NUMBER_PROPERTY, 20);

    RunCommandToFd(fd, "MKDIR GPS LOG", {"/vendor/bin/mkdir", "-p", gpsDestDir.c_str()},
                   CommandOptions::WithTimeout(2).Build());

    dumpLogs(fd, gpsTmpLogDir, gpsDestDir, 1, GPS_LOG_PREFIX);
    dumpLogs(fd, gpsLogDir, gpsDestDir, 3, GPS_MCU_LOG_PREFIX);
    dumpLogs(fd, gpsLogDir, gpsDestDir, maxFileNum, GPS_LOG_PREFIX);
}

void Dumpstate::dumpGxpLogs(int fd, const std::string &destDir) {
    bool gxpDumpEnabled = ::android::base::GetBoolProperty("vendor.gxp.attach_to_bugreport", false);

    if (gxpDumpEnabled) {
        const int maxGxpDebugDumps = 8;
        const std::string gxpCoredumpOutputDir = destDir + "/gxp_ssrdump";
        const std::string gxpCoredumpInputDir = "/data/vendor/ssrdump";

        RunCommandToFd(fd, "MKDIR GXP COREDUMP", {"/vendor/bin/mkdir", "-p", gxpCoredumpOutputDir}, CommandOptions::WithTimeout(2).Build());

        // Copy GXP coredumps and crashinfo to the output directory.
        dumpLogs(fd, gxpCoredumpInputDir + "/coredump", gxpCoredumpOutputDir, maxGxpDebugDumps, "coredump_gxp_platform");
        dumpLogs(fd, gxpCoredumpInputDir, gxpCoredumpOutputDir, maxGxpDebugDumps, "crashinfo_gxp_platform");
    }
}

void Dumpstate::dumpLogSection(int fd, int fd_bin)
{
    std::string logDir = MODEM_LOG_DIRECTORY;
    const std::string logCombined = logDir + "/combined_logs.tar";
    const std::string logAllDir = logDir + "/all_logs";

    RunCommandToFd(fd, "MKDIR LOG", {"/vendor/bin/mkdir", "-p", logAllDir.c_str()}, CommandOptions::WithTimeout(2).Build());

    dumpTextSection(fd, kAllSections);

    // Dump all module logs
    if (!PropertiesHelper::IsUserBuild()) {
        for (const auto &section : mLogSections) {
            auto startTime = startSection(fd, section.first);
            section.second(fd, logAllDir);
            endSection(fd, section.first, startTime);
        }
    }

    RunCommandToFd(fd, "TAR LOG", {"/vendor/bin/tar", "cvf", logCombined.c_str(), "-C", logAllDir.c_str(), "."}, CommandOptions::WithTimeout(20).Build());
    RunCommandToFd(fd, "CHG PERM", {"/vendor/bin/chmod", "a+w", logCombined.c_str()}, CommandOptions::WithTimeout(2).Build());

    std::vector<uint8_t> buffer(65536);
    ::android::base::unique_fd fdLog(TEMP_FAILURE_RETRY(open(logCombined.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK)));

    if (fdLog >= 0) {
        while (1) {
            ssize_t bytes_read = TEMP_FAILURE_RETRY(read(fdLog, buffer.data(), buffer.size()));

            if (bytes_read == 0) {
                break;
            } else if (bytes_read < 0) {
                ALOGD("read(%s): %s\n", logCombined.c_str(), strerror(errno));
                break;
            }

            ssize_t result = TEMP_FAILURE_RETRY(write(fd_bin, buffer.data(), bytes_read));

            if (result != bytes_read) {
                ALOGD("Failed to write %ld bytes, actually written: %ld", bytes_read, result);
                break;
            }
        }
    }

    RunCommandToFd(fd, "RM LOG DIR", { "/vendor/bin/rm", "-r", logAllDir.c_str()}, CommandOptions::WithTimeout(2).Build());
    RunCommandToFd(fd, "RM LOG", { "/vendor/bin/rm", logCombined.c_str()}, CommandOptions::WithTimeout(2).Build());
}

void Dumpstate::dumpPixelTraceSection(int fd) {
    DumpFileToFd(fd, "Pixel trace", "/sys/kernel/tracing/instances/pixel/trace");
}

ndk::ScopedAStatus Dumpstate::dumpstateBoard(const std::vector<::ndk::ScopedFileDescriptor>& in_fds,
                                             IDumpstateDevice::DumpstateMode in_mode,
                                             int64_t in_timeoutMillis) {
    ATRACE_BEGIN("dumpstateBoard");
    // Unused arguments.
    (void) in_timeoutMillis;
    (void) in_mode;

    if (in_fds.size() < 1) {
        ALOGE("no FDs\n");
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "No file descriptor");
    }

    int fd = in_fds[0].get();
    if (fd < 0) {
        ALOGE("invalid FD: %d\n", fd);
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid file descriptor");
    }

    if (in_fds.size() < 2) {
          ALOGE("no FD for dumpstate_board binary\n");
    } else {
          int fd_bin = in_fds[1].get();
          dumpLogSection(fd, fd_bin);
    }

    ATRACE_END();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Dumpstate::setVerboseLoggingEnabled(bool in_enable) {
    ::android::base::SetProperty(kVerboseLoggingProperty, in_enable ? "true" : "false");
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Dumpstate::getVerboseLoggingEnabled(bool* _aidl_return) {
    *_aidl_return = ::android::base::GetBoolProperty(kVerboseLoggingProperty, false);
    return ndk::ScopedAStatus::ok();
}

// Since AIDLs that support the dump() interface are automatically invoked during
// bugreport generation and we don't want to generate a second copy of the same
// data that will go into dumpstate_board.txt, this function will only do
// something if it is called with an option, e.g.
//   dumpsys android.hardware.dumpstate.IDumpstateDevice/default all
//
// Also, note that sections which generate attachments and/or binary data when
// included in a bugreport are not available through the dump() interface.
binder_status_t Dumpstate::dump(int fd, const char** args, uint32_t numArgs) {

    if (numArgs != 1) {
        return STATUS_OK;
    }

    dumpTextSection(fd, static_cast<std::string>(args[0]));

    fsync(fd);
    return STATUS_OK;
}

}  // namespace dumpstate
}  // namespace hardware
}  // namespace android
}  // namespace aidl
