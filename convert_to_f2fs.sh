#!/bin/sh

BLK=$1
FORMAT_ONLY=$2
MNT_OLD="/mnt/convert_old"
MNT_NEW="/mnt/convert_new"

function log() {
	if [ ! -z "$1" ]; then
		echo "partition_convert: $1" > /dev/kmsg
	fi
}

function check_success() {
	if [ $? -ne 0 ]; then
		log "Failed: $1"
		exit -1
	fi
	log "Success: $1"
}

F2FS_MAGIC=$(xxd $BLK -s 0x400 -l 4 -p)
if [ "$F2FS_MAGIC" = "1020f5f2" ]; then
	log "$BLK is already f2fs - skipping"
	exit 0
fi

EXT4_MAGIC=$(xxd $BLK -s 0x438 -l 2 -p)
if [ "$EXT4_MAGIC" != "53ef" ]; then
	log "Unknown filesystem $EXT4_MAGIC"
	exit -1
fi

if [ $FORMAT_ONLY == "true" ]; then
	make_f2fs $BLK 2>&1 > /dev/null
	check_success "make_f2fs $BLK"
	sync
	check_success "sync"
	exit 0
fi

mkdir -p $MNT_OLD
check_success "mkdir $MNT_OLD"

mkdir -p $MNT_NEW
check_success "mkdir $MNT_NEW"

mount -t ext4 $BLK $MNT_OLD
check_success "mount $MNT_OLD as ext4"

mount -t tmpfs tmpfs $MNT_NEW
check_success "mount $MNT_NEW as tmpfs"

cp -rp $MNT_OLD/* $MNT_NEW/
check_success "copy $MNT_OLD to $MNT_NEW"

umount $MNT_OLD
check_success "umount $MNT_OLD"

make_f2fs $BLK 2>&1 > /dev/null
check_success "make_f2fs $BLK"

mount -t f2fs $BLK $MNT_OLD
check_success "mount $MNT_OLD as f2fs"

cp -rp $MNT_NEW/* $MNT_OLD/
check_success "copy $MNT_NEW to $MNT_OLD"

sync
check_success "sync"

umount $MNT_OLD
check_success "umount $MNT_OLD"
umount $MNT_NEW
check_success "umount $MNT_NEW"

rmdir $MNT_OLD
check_success "rmdir $MNT_OLD"

rmdir $MNT_NEW
check_success "rmdir $MNT_NEW"
