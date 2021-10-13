#!/bin/bash

# This script is used to generate uwb conuntry configuration file,
# and the PRODUCT_COPY_FILES list in uwb.mk based on uwb_country.conf
# Bug: 196073172

project=("r4" "p7")
count=1

while read line ; do
    if [[ "$line" =~ ^"*" ]]; then
        header=${line:1}
    elif [[ "$line" =~ ^"\"" ]]; then
        line=$(echo ${line/,} | tr -d "\"")
        country[count]=$(echo $line | cut -d ':' -f1)
        code[count]=$(echo $line | cut -d ':' -f2 | tr -d " ")

        for var in ${project[@]}; do
            if [ "$header" = "restricted_channels=0x20" ]; then
                echo "alternate_pulse_shape=0x01" > UWB-calibration-${code[$count]}.conf.$var
                echo "$header" >> UWB-calibration-${code[$count]}.conf.$var
            else
            echo "$header" > UWB-calibration-${code[$count]}.conf.$var
            fi
        done
    fi
((count++))
done < uwb_country.conf

for var in ${project[@]}; do
    echo "==============  $var  =============="
    for var2 in ${code[@]}; do
        if [ "$var2" = "${code[-1]}" ]; then
            echo "\$(LOCAL_UWB_CAL_DIR)/UWB-calibration-$var2.conf.$var:\$(TARGET_COPY_OUT_VENDOR)/etc/UWB-calibration-$var2.conf"
        else
            echo "\$(LOCAL_UWB_CAL_DIR)/UWB-calibration-$var2.conf.$var:\$(TARGET_COPY_OUT_VENDOR)/etc/UWB-calibration-$var2.conf \\"
        fi
    done
done
