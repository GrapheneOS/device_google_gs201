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

#define LOG_TAG "battery-mitigation"

#include <battery_mitigation/BatteryMitigation.h>

using android::hardware::google::pixel::BatteryMitigation;
using android::hardware::google::pixel::MitigationConfig;

android::sp<BatteryMitigation> bmSp;

const struct MitigationConfig::Config cfg = {
    .SystemPath = {
        "/dev/thermal/tz-by-name/batoilo/temp",
        "/dev/thermal/tz-by-name/smpl_gm/temp",
        "/dev/thermal/tz-by-name/soc/temp",
        "/dev/thermal/tz-by-name/vdroop1/temp",
        "/dev/thermal/tz-by-name/vdroop2/temp",
        "/dev/thermal/tz-by-name/ocp_gpu/temp",
        "/dev/thermal/tz-by-name/ocp_tpu/temp",
        "/dev/thermal/tz-by-name/soft_ocp_cpu2/temp",
        "/dev/thermal/tz-by-name/soft_ocp_cpu1/temp",
        "/dev/thermal/tz-by-name/battery/temp",
        "/dev/thermal/tz-by-name/battery_cycle/temp",
        "/sys/bus/iio/devices/iio:device0/lpf_power",
        "/sys/bus/iio/devices/iio:device1/lpf_power",
        "/dev/thermal/cdev-by-name/thermal-cpufreq-2/cur_state",
        "/dev/thermal/cdev-by-name/thermal-cpufreq-1/cur_state",
        "/dev/thermal/cdev-by-name/thermal-gpufreq-0/cur_state",
        "/dev/thermal/cdev-by-name/tpu_cooling/cur_state",
        "/dev/thermal/cdev-by-name/CAM/cur_state",
        "/dev/thermal/cdev-by-name/DISP/cur_state",
        "/dev/thermal/cdev-by-name/gxp-cooling/cur_state",
        "/sys/class/power_supply/battery/voltage_now",
        "/sys/class/power_supply/battery/current_now",
    },
    .FilteredZones = {
        "batoilo",
        "vdroop1",
        "vdroop2",
        "smpl_gm",
    },
    .SystemName = {
        "batoilo", "smpl_gm", "soc", "vdroop1", "vdroop2", "ocp_gpu",
        "ocp_tpu", "soft_ocp_cpu2", "soft_ocp_cpu1", "battery", "battery_cycle",
        "main", "sub", "CPU2", "CPU1", "GPU", "TPU", "CAM", "DISP", "NPU",
        "voltage_now", "current_now",
    },
    .LogFilePath = "/data/vendor/mitigation/thismeal.txt",
};

int main(int /*argc*/, char ** /*argv*/) {
    bmSp = new BatteryMitigation(cfg);
    while (true) {
        pause();
    }
    return 0;
}
