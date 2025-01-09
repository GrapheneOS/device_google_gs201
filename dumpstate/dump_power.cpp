/*
 * Copyright 2023 The Android Open Source Project
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
#include <cstring>
#include <dirent.h>
#include <dump/pixel_dump.h>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <vector>
#include <android-base/file.h>
#include <android-base/strings.h>
#include "DumpstateUtil.h"
void printTitle(const char *msg) {
    printf("\n------ %s ------\n", msg);
}
int getCommandOutput(const char *cmd, std::string *output) {
    char buffer[1024];
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        return -1;
    }
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
        *output += buffer;
    }
    pclose(pipe);
    if (output->back() == '\n')
        output->pop_back();
    return 0;
}
bool isValidFile(const char *file) {
    FILE *fp = fopen(file, "r");
    if (fp != NULL) {
        fclose(fp);
        return true;
    }
    return false;
}
bool isValidDir(const char *directory) {
    DIR *dir = opendir(directory);
    if (dir == NULL)
        return false;

    closedir(dir);
    return true;
}
bool isUserBuild() {
    return ::android::os::dumpstate::PropertiesHelper::IsUserBuild();
}
int getFilesInDir(const char *directory, std::vector<std::string> *files) {
    std::string content;
    struct dirent *entry;
    DIR *dir = opendir(directory);
    if (dir == NULL)
        return -1;
    files->clear();
    while ((entry = readdir(dir)) != NULL)
        files->push_back(entry->d_name);
    closedir(dir);
    sort(files->begin(), files->end());
    return 0;
}
void dumpPowerStatsTimes() {
    const char *title = "Power Stats Times";
    char rBuff[128];
    struct timespec rTs;
    struct sysinfo info;
    int ret;
    printTitle(title);
    sysinfo(&info);
    const time_t boottime = time(NULL) - info.uptime;
    ret = clock_gettime(CLOCK_REALTIME, &rTs);
    if (ret)
        return;
    struct tm *nowTime = std::localtime(&rTs.tv_sec);
    std::strftime(rBuff, sizeof(rBuff), "%m/%d/%Y %H:%M:%S", nowTime);
    printf("Boot: %s", ctime(&boottime));
    printf("Now: %s\n", rBuff);
}
int readContentsOfDir(const char* title, const char* directory, const char* strMatch,
        bool useStrMatch = false, bool printDirectory = false) {
    std::vector<std::string> files;
    std::string content;
    std::string fileLocation;
    int ret;
    ret = getFilesInDir(directory, &files);
    if (ret < 0)
        return ret;
    printTitle(title);
    for (auto &file : files) {
        if (useStrMatch && std::string::npos == std::string(file).find(strMatch)) {
            continue;
        }
        fileLocation = std::string(directory) + std::string(file);
        if (!android::base::ReadFileToString(fileLocation, &content)) {
            continue;
        }
        if (printDirectory) {
            printf("\n\n%s\n", fileLocation.c_str());
        }
        if (content.back() == '\n')
            content.pop_back();
        printf("%s\n", content.c_str());
    }
    return 0;
}
void dumpAcpmStats() {
    const char* acpmDir = "/sys/devices/platform/acpm_stats/";
    const char* statsSubStr = "_stats";
    const char* acpmTitle = "ACPM stats";
    readContentsOfDir(acpmTitle, acpmDir, statsSubStr, true, true);
}
void dumpPowerSupplyStats() {
    const char* dumpList[][2] = {
            {"CPU PM stats", "/sys/devices/system/cpu/cpupm/cpupm/time_in_state"},
            {"Power supply property battery", "/sys/class/power_supply/battery/uevent"},
            {"Power supply property dc", "/sys/class/power_supply/dc/uevent"},
            {"Power supply property gcpm", "/sys/class/power_supply/gcpm/uevent"},
            {"Power supply property gcpm_pps", "/sys/class/power_supply/gcpm_pps/uevent"},
            {"Power supply property main-charger", "/sys/class/power_supply/main-charger/uevent"},
            {"Power supply property pca94xx-mains", "/sys/class/power_supply/pca94xx-mains/uevent"},
            {"Power supply property tcpm", "/sys/class/power_supply/tcpm-source-psy-i2c-max77759tcpc/uevent"},
            {"Power supply property usb", "/sys/class/power_supply/usb/uevent"},
            {"Power supply property wireless", "/sys/class/power_supply/wireless/uevent"},
    };
    for (const auto &row : dumpList) {
        dumpFileContent(row[0], row[1]);
    }
}
void dumpMaxFg() {
    const char *maxfgLoc = "/sys/class/power_supply/maxfg";
    const char *maxfgDualLoc = "/sys/class/power_supply/maxfg_base";
    const char *maxfg [][2] = {
            {"Power supply property maxfg", "/sys/class/power_supply/maxfg/uevent"},
            {"m5_state", "/sys/class/power_supply/maxfg/m5_model_state"},
            {"maxfg registers", "/sys/class/power_supply/maxfg/registers_dump"},
            {"maxfg logbuffer", "/dev/logbuffer_maxfg"},
            {"maxfg_monitor logbuffer", "/dev/logbuffer_maxfg_monitor"},
    };
    const char *maxfgDual [][2] = {
            {"Power supply property maxfg_base", "/sys/class/power_supply/maxfg_base/uevent"},
            {"Power supply property maxfg_secondary", "/sys/class/power_supply/maxfg_secondary/uevent"},
            {"maxfg_base registers", "/sys/class/power_supply/maxfg_base/registers_dump"},
            {"maxfg_secondary registers", "/sys/class/power_supply/maxfg_secondary/registers_dump"},
            {"m5_state", "/sys/class/power_supply/maxfg_base/m5_model_state"},
            {"maxfg_base logbuffer", "/dev/logbuffer_maxfg_base"},
            {"maxfg_secondary logbuffer", "/dev/logbuffer_maxfg_secondary"},
            {"maxfg_base logbuffer", "/dev/logbuffer_maxfg_base_monitor"},
            {"maxfg_secondary logbuffer", "/dev/logbuffer_maxfg_secondary_monitor"},
            {"dual_batt logbuffer", "/dev/logbuffer_dual_batt"},
    };
    const char *maxfgHistoryName = "Maxim FG History";
    const char *maxfgHistoryDir = "/dev/maxfg_history";
    std::string content;
    if (isValidDir(maxfgLoc)) {
        for (const auto &row : maxfg) {
            dumpFileContent(row[0], row[1]);
        }
    } else if (isValidDir(maxfgDualLoc)){
        for (const auto &row : maxfgDual) {
            dumpFileContent(row[0], row[1]);
        }
        if (isValidFile(maxfgHistoryDir)) {
            dumpFileContent(maxfgHistoryName, maxfgHistoryDir);
        }
    }
}
void dumpPowerSupplyDock() {
    const char* powerSupplyPropertyDockTitle = "Power supply property dock";
    const char* powerSupplyPropertyDockFile = "/sys/class/power_supply/dock/uevent";
    dumpFileContent(powerSupplyPropertyDockTitle, powerSupplyPropertyDockFile);
}
void dumpLogBufferTcpm() {
    const char* logbufferTcpmTitle = "Logbuffer TCPM";
    const char* logbufferTcpmFile = "/dev/logbuffer_tcpm";
    const char* debugTcpmFile = "/sys/kernel/debug/tcpm";
    const char* tcpmLogTitle = "TCPM logs";
    const char* tcpmFile = "/sys/kernel/debug/tcpm";
    const char* tcpmFileAlt = "/sys/kernel/debug/usb/tcpm";
    int retCode;
    dumpFileContent(logbufferTcpmTitle, logbufferTcpmFile);
    retCode = readContentsOfDir(tcpmLogTitle, isValidFile(debugTcpmFile) ? tcpmFile : tcpmFileAlt,
            NULL);
    if (retCode < 0)
        printTitle(tcpmLogTitle);
}
void dumpTcpc() {
    const char* max77759TcpcHead = "TCPC Device Attributes";
    const char* directory = "/sys/class/typec/port0/device";
    const char* max77759Tcpc [] {
            "auto_discharge",
            "bc12_enabled",
            "cc_toggle_enable",
            "contaminant_detection",
            "contaminant_detection_status",
            "frs",
            "irq_hpd_count",
            "manual_disable_vbus",
            "non_compliant_reasons",
            "sbu_pullup",
            "update_sdp_enum_timeout",
            "usb_limit_accessory_current",
            "usb_limit_accessory_enable",
            "usb_limit_sink_current",
            "usb_limit_sink_enable",
            "usb_limit_source_enable",
    };

    std::string content;
    std::string tcpcRegistersPath(std::string(directory) + "/registers");

    dumpFileContent("TCPC Registers", tcpcRegistersPath.c_str());

    printTitle(max77759TcpcHead);

    for (auto& tcpcVal : max77759Tcpc) {
        std::string filename = std::string(directory) + "/" + std::string(tcpcVal);
        printf("%s: ", tcpcVal);
        android::base::ReadFileToString(filename, &content);
        if (!content.empty() && (content.back() == '\n' || content.back() == '\r'))
            content.pop_back();
        printf("%s\n", content.c_str());
    }
    printf("\n");
}

void dumpPdEngine() {
    const char* pdEngine [][3] {
            {"TCPC logbuffer", "/dev/logbuffer_usbpd"},
            {"pogo_transport logbuffer", "/dev/logbuffer_pogo_transport"},
            {"PPS-google_cpm logbuffer", "/dev/logbuffer_cpm"},
            {"PPS-pca9468 logbuffer", "/dev/logbuffer_pca9468"},
    };
    for (const auto &row : pdEngine) {
        dumpFileContent(row[0], row[1]);
    }
}
void dumpBatteryHealth() {
    const char* batteryHealth [][2] {
            {"Battery Health", "/sys/class/power_supply/battery/health_index_stats"},
            {"Battery Health SoC Residency", "/sys/class/power_supply/battery/swelling_data"},
            {"BMS", "/dev/logbuffer_ssoc"},
            {"TTF", "/dev/logbuffer_ttf"},
            {"TTF details", "/sys/class/power_supply/battery/ttf_details"},
            {"TTF stats", "/sys/class/power_supply/battery/ttf_stats"},
            {"aacr_state", "/sys/class/power_supply/battery/aacr_state"},
            {"maxq", "/dev/logbuffer_maxq"},
            {"TEMP/DOCK-DEFEND", "/dev/logbuffer_bd"},
    };
    for (const auto &row : batteryHealth) {
        dumpFileContent(row[0], row[1]);
    }
}
void dumpBatteryDefend() {
    const char* defendConfig [][3] {
            {"TRICKLE-DEFEND Config",
                    "/sys/devices/platform/google,battery/power_supply/battery/", "bd_"},
            {"DWELL-DEFEND Config", "/sys/devices/platform/google,charger/", "charge_s"},
            {"TEMP-DEFEND Config", "/sys/devices/platform/google,charger/", "bd_"},
    };
    std::vector<std::string> files;
    struct dirent *entry;
    std::string content;
    std::string fileLocation;
    for (auto &config : defendConfig) {
        DIR *dir = opendir(config[1]);
        if (dir == NULL)
            continue;
        printTitle(config[0]);
        while ((entry = readdir(dir)) != NULL) {
            if (std::string(entry->d_name).find(config[2]) != std::string::npos &&
                    strncmp(config[2], entry->d_name, strlen(config[2])) == 0) {
                files.push_back(entry->d_name);
            }
        }
        closedir(dir);
        sort(files.begin(), files.end());
        for (auto &file : files) {
            fileLocation = std::string(config[1]) + std::string(file);
            if (!android::base::ReadFileToString(fileLocation, &content) || content.empty()) {
                content = "\n";
            }
            printf("%s: %s", file.c_str(), content.c_str());
            if (content.back() != '\n')
                printf("\n");
        }
        files.clear();
    }
}
void dumpBatteryCaretaker() {
    const char* aacpConfig [][3] {
            {"AACP Version",
                    "/sys/devices/platform/google,battery/power_supply/battery/", "aacp_"},
            {"AACR Config",
                    "/sys/devices/platform/google,battery/power_supply/battery/", "aacr_"},
            {"AAFV Config",
                    "/sys/devices/platform/google,battery/power_supply/battery/", "aafv_"},
            {"AACT Config",
                    "/sys/devices/platform/google,battery/power_supply/battery/", "aact_"},
            {"AACC",
                    "/sys/devices/platform/google,battery/power_supply/battery/", "aacc"},
    };
    std::vector<std::string> files;
    struct dirent *entry;
    std::string content;
    std::string fileLocation;
    for (auto &config : aacpConfig) {
        DIR *dir = opendir(config[1]);
        if (dir == NULL)
            continue;
        printTitle(config[0]);
        while ((entry = readdir(dir)) != NULL) {
            if (std::string(entry->d_name).find(config[2]) != std::string::npos &&
                    strncmp(config[2], entry->d_name, strlen(config[2])) == 0) {
                files.push_back(entry->d_name);
            }
        }
        closedir(dir);
        sort(files.begin(), files.end());
        for (auto &file : files) {
            fileLocation = std::string(config[1]) + std::string(file);
            if (!android::base::ReadFileToString(fileLocation, &content) || content.empty()) {
                content = "\n";
            }
            printf("%s: %s", file.c_str(), content.c_str());
            if (content.back() != '\n')
                printf("\n");
        }
        files.clear();
    }
}
void printValuesOfDirectory(const char *directory, std::string debugfs, const char *strMatch) {
    std::vector<std::string> files;
    auto info = directory;
    std::string content;
    struct dirent *entry;
    DIR *dir = opendir(debugfs.c_str());
    if (dir == NULL)
        return;

    printTitle((debugfs + std::string(strMatch) + "/" + std::string(info)).c_str());
    while ((entry = readdir(dir)) != NULL)
        if (std::string(entry->d_name).find(strMatch) != std::string::npos)
            files.push_back(entry->d_name);
    closedir(dir);

    sort(files.begin(), files.end());

    for (auto &file : files) {
        std::string fileDirectory = debugfs + file;
        std::string fileLocation = fileDirectory + "/" + std::string(info);
        if (!android::base::ReadFileToString(fileLocation, &content)) {
            content = "\n";
        }

        printf("%s:\n%s", fileDirectory.c_str(), content.c_str());

        if (content.back() != '\n')
            printf("\n");
    }
    files.clear();
}
void dumpChg() {
    const std::string pmic_bus = "/sys/devices/platform/10d60000.hsi2c/i2c-13/13-0066";
    const char* chg_reg_dump_file = "/sys/class/power_supply/main-charger/device/registers_dump";
    const std::string chg_name_cmd = "/sys/class/power_supply/main-charger/device/name";
    const std::string pmic_name_cmd = pmic_bus + "/name";
    const std::string pmic_reg_dump_file = pmic_bus + "/registers_dump";
    const std::string reg_dump_str = " registers dump";
    const char* chgConfig [][2] {
        {"DC_registers dump", "/sys/class/power_supply/pca94xx-mains/device/registers_dump"},
    };
    std::string chg_name;
    std::string pmic_name;

    printf("\n");

    int ret = android::base::ReadFileToString(chg_name_cmd, &chg_name);
    if (ret && !chg_name.empty()) {
        chg_name.erase(chg_name.length() - 1); // remove new line
        const std::string chg_reg_dump_title = chg_name + reg_dump_str;

        /* CHG reg dump */
        dumpFileContent(chg_reg_dump_title.c_str(), chg_reg_dump_file);
    }

    ret = android::base::ReadFileToString(pmic_name_cmd, &pmic_name);
    if (ret && !pmic_name.empty()) {
        pmic_name.erase(pmic_name.length() - 1); // remove new line
        const std::string pmic_reg_dump_title = pmic_name + reg_dump_str;

        /* PMIC reg dump */
        dumpFileContent(pmic_reg_dump_title.c_str(), pmic_reg_dump_file.c_str());
    }

    for (auto &config : chgConfig) {
        dumpFileContent(config[0], config[1]);
    }
}
void dumpChgUserDebug() {
    const std::string debugfs = "/d/";
    const char *maxFgDir = "/d/maxfg";
    const char *maxFgStrMatch = "maxfg";
    const char *maxBaseFgDir = "/d/maxfg_base";
    const char *maxBaseFgStrMatch = "maxfg_base";
    const char *chgTblName = "Charging table dump";
    const char *chgTblDir = "/d/google_battery/chg_raw_profile";

    const char *maxFgInfo [] {
            "fg_model",
            "algo_ver",
            "model_ok",
    };

    if (isUserBuild())
        return;

    dumpFileContent(chgTblName, chgTblDir);

    if (isValidDir(maxFgDir)) {
        for (auto & directory : maxFgInfo) {
            printValuesOfDirectory(directory, debugfs, maxFgStrMatch);
        }
    } else if (isValidDir(maxBaseFgDir)) {
        for (auto & directory : maxFgInfo) {
            printValuesOfDirectory(directory, debugfs, maxBaseFgStrMatch);
        }
    }
}
void dumpBatteryEeprom() {
    const char *title = "Battery EEPROM";
    const char *files[] {
            "/sys/devices/platform/10da0000.hsi2c/i2c-15/15-0050/eeprom",
    };
    std::string result;
    std::string xxdCmd;
    printTitle(title);
    for (auto &file : files) {
        if (!isValidFile(file))
            continue;
        xxdCmd = "xxd " + std::string(file);
        int ret = getCommandOutput(xxdCmd.c_str(), &result);
        if (ret < 0)
            return;
        printf("%s\n", result.c_str());
    }
}
void dumpChargerStats() {
    const char *chgStatsTitle = "Charger Stats";
    const char *chgStatsLocation = "/sys/class/power_supply/battery/charge_details";
    const char *chargerStats [][3] {
            {"Google Charger", "/sys/kernel/debug/google_charger/", "pps_"},
            {"Google Battery", "/sys/kernel/debug/google_battery/", "ssoc_"},
    };
    std::vector<std::string> files;
    std::string content;
    struct dirent *entry;
    dumpFileContent(chgStatsTitle, chgStatsLocation);
    if (isUserBuild())
        return;
    for (auto &stat : chargerStats) {
        DIR *dir = opendir(stat[1]);
        if (dir == NULL)
            return;
        printTitle(stat[0]);
        while ((entry = readdir(dir)) != NULL)
            if (std::string(entry->d_name).find(stat[2]) != std::string::npos)
                files.push_back(entry->d_name);
        closedir(dir);
        sort(files.begin(), files.end());
        for (auto &file : files) {
            std::string fileLocation = std::string(stat[1]) + file;
            if (!android::base::ReadFileToString(fileLocation, &content)) {
                content = "\n";
            }
            printf("%s: %s", file.c_str(), content.c_str());
            if (content.back() != '\n')
                printf("\n");
        }
        files.clear();
    }
}
void dumpWlcLogs() {
    const char *dumpWlcList [][2] {
            {"WLC Logs", "/dev/logbuffer_wireless"},
            {"WLC VER", "/sys/class/power_supply/wireless/device/version"},
            {"WLC STATUS", "/sys/class/power_supply/wireless/device/status"},
            {"WLC FW Version", "/sys/class/power_supply/wireless/device/fw_rev"},
            {"RTX", "/dev/logbuffer_rtx"},
    };
    for (auto &row : dumpWlcList) {
        if (!isValidFile(row[1]))
            printTitle(row[0]);
        dumpFileContent(row[0], row[1]);
    }
}
void dumpGvoteables() {
    const char *directory = "/sys/kernel/debug/gvotables/";
    const char *statusName = "/status";
    const char *title = "gvotables";
    std::string content;
    std::vector<std::string> files;
    int ret;
    if (isUserBuild())
        return;
    ret = getFilesInDir(directory, &files);
    if (ret < 0)
        return;
    printTitle(title);
    for (auto &file : files) {
        std::string fileLocation = std::string(directory) + file + std::string(statusName);
        if (!android::base::ReadFileToString(fileLocation, &content)) {
            continue;
        }
        printf("%s: %s", file.c_str(), content.c_str());
        if (content.back() != '\n')
            printf("\n");
    }
    files.clear();
}
void dumpMitigation() {
    const char *mitigationList [][2] {
            {"Lastmeal" , "/data/vendor/mitigation/lastmeal.txt"},
            {"Thismeal" , "/data/vendor/mitigation/thismeal.txt"},
    };
    for (auto &row : mitigationList) {
        if (!isValidFile(row[1]))
            printTitle(row[0]);
        dumpFileContent(row[0], row[1]);
    }
}
void dumpMitigationStats() {
    int ret;
    const char *directory = "/sys/devices/virtual/pmic/mitigation/last_triggered_count/";
    const char *capacityDirectory = "/sys/devices/virtual/pmic/mitigation/last_triggered_capacity/";
    const char *timestampDirectory =
            "/sys/devices/virtual/pmic/mitigation/last_triggered_timestamp/";
    const char *voltageDirectory = "/sys/devices/virtual/pmic/mitigation/last_triggered_voltage/";
    const char *capacitySuffix = "_cap";
    const char *timeSuffix = "_time";
    const char *voltageSuffix = "_volt";
    const char *countSuffix = "_count";
    const char *title = "Mitigation Stats";
    std::vector<std::string> files;
    std::string content;
    std::string fileLocation;
    std::string source;
    std::string subModuleName;
    int count;
    int soc;
    int time;
    int voltage;
    ret = getFilesInDir(directory, &files);
    if (ret < 0)
        return;
    printTitle(title);
    printf("Source\t\tCount\tSOC\tTime\tVoltage\n");
    for (auto &file : files) {
        fileLocation = std::string(directory) + std::string(file);
        if (!android::base::ReadFileToString(fileLocation, &content)) {
            continue;
        }
        ret = atoi(android::base::Trim(content).c_str());
        if (ret == -1)
            continue;
        count = ret;
        subModuleName = std::string(file);
        subModuleName.erase(subModuleName.find(countSuffix), strlen(countSuffix));
        fileLocation = std::string(capacityDirectory) + std::string(subModuleName) +
                std::string(capacitySuffix);
        if (!android::base::ReadFileToString(fileLocation, &content)) {
            continue;
        }
        ret = atoi(android::base::Trim(content).c_str());
        if (ret == -1)
            continue;
        soc = ret;
        fileLocation = std::string(timestampDirectory) + std::string(subModuleName) +
                std::string(timeSuffix);
        if (!android::base::ReadFileToString(fileLocation, &content)) {
            continue;
        }
        ret = atoi(android::base::Trim(content).c_str());
        if (ret == -1)
            continue;
        time = ret;
        fileLocation = std::string(voltageDirectory) + std::string(subModuleName) +
                std::string(voltageSuffix);
        if (!android::base::ReadFileToString(fileLocation, &content)) {
            continue;
        }
        ret = atoi(android::base::Trim(content).c_str());
        if (ret == -1)
            continue;
        voltage = ret;
        printf("%s \t%i\t%i\t%i\t%i\n", subModuleName.c_str(), count, soc, time, voltage);
    }
}
void dumpMitigationDirs() {
    const int paramCount = 4;
    const char *titles[] = {
            "Clock Divider Ratio",
            "Clock Stats",
            "Triggered Level",
            "Instruction",
    };
    const char *directories[] = {
            "/sys/devices/virtual/pmic/mitigation/clock_ratio/",
            "/sys/devices/virtual/pmic/mitigation/clock_stats/",
            "/sys/devices/virtual/pmic/mitigation/triggered_lvl/",
            "/sys/devices/virtual/pmic/mitigation/instruction/",
    };
    const char *paramSuffix[] = {"_ratio", "_stats", "_lvl", ""};
    const char *titleRowVal[] = {
            "Source\t\tRatio",
            "Source\t\tStats",
            "Source\t\tLevel",
            "",
    };
    const int eraseCnt[] = {6, 6, 4, 0};
    const bool useTitleRow[] = {true, true, true, false};
    std::vector<std::string> files;
    std::string content;
    std::string fileLocation;
    std::string source;
    std::string subModuleName;
    std::string readout;
    for (int i = 0; i < paramCount; i++) {
        printTitle(titles[i]);
        if (useTitleRow[i]) {
            printf("%s\n", titleRowVal[i]);
        }
        getFilesInDir(directories[i], &files);
        for (auto &file : files) {
            fileLocation = std::string(directories[i]) + std::string(file);
            if (!android::base::ReadFileToString(fileLocation, &content)) {
                continue;
            }
            readout = android::base::Trim(content);
            subModuleName = std::string(file);
            subModuleName.erase(subModuleName.find(paramSuffix[i]), eraseCnt[i]);
            if (useTitleRow[i]) {
                printf("%s \t%s\n", subModuleName.c_str(), readout.c_str());
            } else {
                printf("%s=%s\n", subModuleName.c_str(), readout.c_str());
            }
        }
    }
}
void dumpIrqDurationCounts() {
    const char *title = "IRQ Duration Counts";
    const char *colNames = "Source\t\t\t\tlt_5ms_cnt\tbt_5ms_to_10ms_cnt\tgt_10ms_cnt\tCode"
            "\tCurrent Threshold (uA)\tCurrent Reading (uA)\n";
    const int nonOdpmChannelCnt = 9;
    const int odpmChCnt = 12;
    enum Duration {
        LT_5MS,
        BT_5MS_10MS,
        GT_10MS,
        DUR_MAX,
    };
    const char *irqDurDirectories[] = {
            "/sys/devices/virtual/pmic/mitigation/irq_dur_cnt/less_than_5ms_count",
            "/sys/devices/virtual/pmic/mitigation/irq_dur_cnt/between_5ms_to_10ms_count",
            "/sys/devices/virtual/pmic/mitigation/irq_dur_cnt/greater_than_10ms_count",
    };
    enum PowerWarn {
        MAIN,
        SUB,
        PWRWARN_MAX,
    };
    const char *pwrwarnDirectories[] = {
            "/sys/devices/virtual/pmic/mitigation/main_pwrwarn/",
            "/sys/devices/virtual/pmic/mitigation/sub_pwrwarn/",
    };
    const char *lpfCurrentDirs[] = {
            "/sys/devices/platform/acpm_mfd_bus@15500000/i2c-1/1-001f/s2mpg14-meter/"
                    "s2mpg14-odpm/iio:device1/lpf_current",
            "/sys/devices/platform/acpm_mfd_bus@15510000/i2c-0/0-002f/s2mpg15-meter/"
                    "s2mpg15-odpm/iio:device0/lpf_current",
    };
    bool titlesInitialized = false;
    std::vector<std::string> channelNames;
    std::vector<std::string> channelData[DUR_MAX];
    std::vector<std::string> pwrwarnThreshold[PWRWARN_MAX];
    std::vector<std::string> pwrwarnCode[PWRWARN_MAX];
    std::vector<std::string> lpfCurrentVals[PWRWARN_MAX];
    std::vector<std::string> files;
    std::string content;
    std::string token;
    std::string tokenCh;
    std::string fileLocation;
    for (int i = 0; i < DUR_MAX; i++) {
        if (!android::base::ReadFileToString(irqDurDirectories[i], &content)) {
            return;
        }
        std::istringstream tokenStream(content);
        while (std::getline(tokenStream, token, '\n')) {
            if (!titlesInitialized) {
                tokenCh = token;
                tokenCh.erase(tokenCh.find(':'), tokenCh.length());
                channelNames.push_back(tokenCh);
            }
            // there is a space after the ':' which needs to be removed
            token.erase(0, token.find(':') + 1);
            channelData[i].push_back(token);
        }
        if (!titlesInitialized)
            titlesInitialized = true;
    }
    for (int i = 0; i < PWRWARN_MAX; i++) {
        getFilesInDir(pwrwarnDirectories[i], &files);
        for (auto &file : files) {
            fileLocation = std::string(pwrwarnDirectories[i]) + std::string(file);
            if (!android::base::ReadFileToString(fileLocation, &content)) {
                continue;
            }
            std::string readout;
            readout = android::base::Trim(content);
            std::string readoutThreshold = readout;
            readoutThreshold.erase(0, readoutThreshold.find('=') + 1);
            std::string readoutCode = readout;
            readoutCode.erase(readoutCode.find('='), readoutCode.length());
            pwrwarnThreshold[i].push_back(readoutThreshold);
            pwrwarnCode[i].push_back(readoutCode);
        }
    }
    for (int i = 0; i < PWRWARN_MAX; i++) {
        if (!android::base::ReadFileToString(lpfCurrentDirs[i], &content)) {
            continue;
        }
        std::istringstream tokenStream(content);
        bool first = true;
        while (std::getline(tokenStream, token, '\n')) {
            token.erase(0, token.find(' '));
            if (first) {
                first = false;
                continue;
            }
            lpfCurrentVals[i].push_back(token);
        }
    }
    printTitle(title);
    printf("%s", colNames);
    for (uint i = 0; i < channelNames.size(); i++) {
        std::string code = "";
        std::string threshold = "";
        std::string current = "";
        std::string ltDataMsg = "";
        std::string btDataMsg = "";
        std::string gtDataMsg = "";
        int pmicSel = 0;
        int offset = 0;
        std::string channelNameSuffix = "      \t";
        if (i >= nonOdpmChannelCnt) {
            offset = nonOdpmChannelCnt;
            if (i >= (odpmChCnt + nonOdpmChannelCnt)) {
                pmicSel = 1;
                offset = odpmChCnt + nonOdpmChannelCnt;
            }
            channelNameSuffix = "";
            code = pwrwarnCode[pmicSel][i - offset];
            threshold = pwrwarnThreshold[pmicSel][i - offset];
            current = lpfCurrentVals[pmicSel][i - offset];
        }
        if (i < channelData[0].size())
            ltDataMsg = channelData[0][i];
        if (i < channelData[1].size())
            btDataMsg = channelData[1][i];
        if (i < channelData[2].size())
            gtDataMsg = channelData[2][i];
        std::string adjustedChannelName = channelNames[i] + channelNameSuffix;
        printf("%s     \t%s\t\t%s\t\t\t%s\t\t%s    \t%s       \t\t%s\n",
                adjustedChannelName.c_str(),
                ltDataMsg.c_str(),
                btDataMsg.c_str(),
                gtDataMsg.c_str(),
                code.c_str(),
                threshold.c_str(),
                current.c_str());
    }
}
int main() {
    dumpPowerStatsTimes();
    dumpAcpmStats();
    dumpPowerSupplyStats();
    dumpMaxFg();
    dumpPowerSupplyDock();
    dumpLogBufferTcpm();
    dumpTcpc();
    dumpPdEngine();
    dumpBatteryHealth();
    dumpBatteryDefend();
    dumpBatteryCaretaker();
    dumpChg();
    dumpChgUserDebug();
    dumpBatteryEeprom();
    dumpChargerStats();
    dumpWlcLogs();
    dumpGvoteables();
    dumpMitigation();
    dumpMitigationStats();
    dumpMitigationDirs();
    dumpIrqDurationCounts();
}
