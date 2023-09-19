#!/vendor/bin/sh
#
# The script belongs to the feature of UFS FFU via OTA: go/p23-ffu-ota
# Its purpose is to copy the corresponding firmware into partition for UFS FFU.

ufs_dev="/dev/sys/block/bootdevice"
fw_dir="/vendor/firmware"
blk_dev="/dev/block/by-name/fips"
min_timestamp=$(date -d "2022-10-01" +%s)
epoch_1y=$(date -u -d "1971-01-01" +%s)

vendor=$(cat ${ufs_dev}/vendor | tr -d "[:space:]")
model=$(cat ${ufs_dev}/model | tr -d "[:space:]")
rev=$(cat ${ufs_dev}/rev | tr -d "[:space:]")

file=$(find ${fw_dir} -name "*${vendor}${model}${rev}*" | head -n 1)
lifec=$(cat ${ufs_dev}/health_descriptor/life_time_estimation_c | tr -d "[:space:]")
[ ! "$lifec" ] && lifec=0xff

atime=$(ls -lu -d /data 2>&1 | awk '{print $6}')
timestamp=$(date -d "$atime" +%s)
(( $timestamp < $min_timestamp )) && timestamp=$min_timestamp
now=$(date +%s)

if [ -n "$file" ] && (( $lifec < 0x0a )) && (( $timestamp + $epoch_1y >= $now )); then
	# The first 4KB block at fips partition has been occupied, and unused space begins from 4 KB
	# Refer to: go/pixel-mp-ffu-ota-1p
	dd if="$file" of=$blk_dev bs=4k seek=1
fi
