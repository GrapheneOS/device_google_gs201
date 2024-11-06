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
    if (!access(file, R_OK)) {
        return false;
    }
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
            {"Power supply property dc-mains", "/sys/class/power_supply/dc-mains/uevent"},
            {"Power supply property tcpm", "/sys/class/power_supply/tcpm-source-psy-8-0025/uevent"},
            {"Power supply property usb", "/sys/class/power_supply/usb/uevent"},
            {"Power supply property wireless", "/sys/class/power_supply/wireless/uevent"},
    };
    for (const auto &row : dumpList) {
        dumpFileContent(row[0], row[1]);
    }
}
void dumpMaxFg() {
    const char *maxfgLoc = "/sys/class/power_supply/maxfg";
    const char *maxfg [][2] = {
            {"Power supply property maxfg", "/sys/class/power_supply/maxfg/uevent"},
            {"m5_state", "/sys/class/power_supply/maxfg/m5_model_state"},
            {"maxfg", "/dev/logbuffer_maxfg"},
            {"maxfg", "/dev/logbuffer_maxfg_monitor"},
    };
    const char *maxfgFlip [][2] = {
            {"Power supply property maxfg_base", "/sys/class/power_supply/maxfg_base/uevent"},
            {"Power supply property maxfg_flip", "/sys/class/power_supply/maxfg_flip/uevent"},
            {"m5_state", "/sys/class/power_supply/maxfg_base/m5_model_state"},
            {"maxfg_base", "/dev/logbuffer_maxfg_base"},
            {"maxfg_flip", "/dev/logbuffer_maxfg_flip"},
            {"maxfg_base", "/dev/logbuffer_maxfg_base_monitor"},
            {"maxfg_flip", "/dev/logbuffer_maxfg_flip_monitor"},
    };
    const char *maxfgHistoryName = "Maxim FG History";
    const char *maxfgHistoryDir = "/dev/maxfg_history";
    std::string content;
    if (isValidFile(maxfgLoc)) {
        for (const auto &row : maxfg) {
            dumpFileContent(row[0], row[1]);
        }
    } else {
        for (const auto &row : maxfgFlip) {
            dumpFileContent(row[0], row[1]);
        }
    }
    if (isValidFile(maxfgHistoryDir)) {
        dumpFileContent(maxfgHistoryName, maxfgHistoryDir);
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
    int ret;
    const char* max77759TcpcHead = "TCPC";
    const char* i2cSubDirMatch = "i2c-";
    const char* directory = "/sys/devices/platform/10d60000.hsi2c/";
    const char* max77759Tcpc [][2] {
            {"registers:", "/i2c-max77759tcpc/registers"},
            {"frs:", "/i2c-max77759tcpc/frs"},
            {"auto_discharge:", "/i2c-max77759tcpc/auto_discharge"},
            {"bcl2_enabled:", "/i2c-max77759tcpc/bcl2_enabled"},
            {"cc_toggle_enable:", "/i2c-max77759tcpc/cc_toggle_enable"},
            {"containment_detection:", "/i2c-max77759tcpc/containment_detection"},
            {"containment_detection_status:", "/i2c-max77759tcpc/containment_detection_status"},
    };
    std::vector<std::string> files;
    std::string content;
    printTitle(max77759TcpcHead);
    ret = getFilesInDir(directory, &files);
    if (ret < 0) {
        for (auto &tcpcVal : max77759Tcpc)
            printf("%s\n", tcpcVal[0]);
        return;
    }
    for (auto &file : files) {
        for (auto &tcpcVal : max77759Tcpc) {
            printf("%s ", tcpcVal[0]);
            if (std::string::npos == std::string(file).find(i2cSubDirMatch)) {
                continue;
            }
            std::string fileName = directory + file + "/" + std::string(tcpcVal[1]);
            if (!android::base::ReadFileToString(fileName, &content)) {
                continue;
            }
            printf("%s\n", content.c_str());
        }
    }
}
void dumpPdEngine() {
    const char* pdEngine [][2] {
            {"PD Engine", "/dev/logbuffer_usbpd"},
            {"PPS-google_cpm", "/dev/logbuffer_cpm"},
            {"PPS-dc", "/dev/logbuffer_pca9468"},
    };
    for (const auto &row : pdEngine) {
        dumpFileContent(row[0], row[1]);
    }
}
void dumpWc68() {
    const char* wc68Title = "WC68";
    const char* wc68File = "/dev/logbuffer_wc68";
    dumpFileContent(wc68Title, wc68File);
}
void dumpLn8411() {
    const char* ln8411Title = "LN8411";
    const char* ln8411File = "/dev/logbuffer_ln8411";
    dumpFileContent(ln8411Title, ln8411File);
}
void dumpBatteryHealth() {
    const char* batteryHealth [][2] {
            {"Battery Health", "/sys/class/power_supply/battery/health_index_stats"},
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
void dumpBatteryEeprom() {
    const char *title = "Battery EEPROM";
    const char *files[] {
            "/sys/devices/platform/10970000.hsi2c/i2c-4/4-0050/eeprom",
            "/sys/devices/platform/10970000.hsi2c/i2c-5/5-0050/eeprom",
            "/sys/devices/platform/10da0000.hsi2c/i2c-6/6-0050/eeprom",
            "/sys/devices/platform/10da0000.hsi2c/i2c-7/7-0050/eeprom",
            "/sys/devices/platform/10c90000.hsi2c/i2c-7/7-0050/eeprom",
            "/sys/devices/platform/10c90000.hsi2c/i2c-6/6-0050/eeprom",
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
    if (!isUserBuild())
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
    if (!isUserBuild())
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
    dumpWc68();
    dumpLn8411();
    dumpBatteryHealth();
    dumpBatteryDefend();
    dumpBatteryEeprom();
    dumpChargerStats();
    dumpWlcLogs();
    dumpGvoteables();
    dumpMitigation();
    dumpMitigationStats();
    dumpMitigationDirs();
    dumpIrqDurationCounts();
}
