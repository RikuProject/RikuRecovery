#!/bin/sh
# Сбор сведений об устройстве для рекавери. Только чтение, ничего не меняет.
# Запуск на устройстве (adb shell или SSH на сервере с часами):
#   sh probe.sh > probe.txt
# Затем пришлите probe.txt.

sec() { printf '\n===== %s =====\n' "$1"; }
try() { "$@" 2>&1; }
prop() { command -v getprop >/dev/null 2>&1 && getprop "$1" || echo "(нет getprop)"; }

sec "Система"
try uname -a
grep -i -E "^(hardware|model name|processor|cpu architecture)" /proc/cpuinfo 2>&1 | sort -u

sec "Свойства Android"
for p in ro.product.model ro.product.device ro.board.platform ro.product.board \
         ro.hardware ro.build.version.release ro.build.fingerprint \
         ro.boot.verifiedbootstate ro.boot.flash.locked ro.boot.slot_suffix \
         ro.boot.hardware ro.bootloader; do
    printf '%s = %s\n' "$p" "$(prop $p)"
done

sec "Разделы (/proc/partitions)"
try cat /proc/partitions

sec "Имена разделов (by-name)"
for d in /dev/block/by-name /dev/block/platform/*/by-name /dev/block/platform/*/*/by-name; do
    [ -d "$d" ] && { echo "$d"; try ls -l "$d"; }
done

sec "Монтирования"
try cat /proc/mounts

sec "Framebuffer"
ls -l /dev/fb* /dev/graphics/fb* 2>&1
for f in /sys/class/graphics/fb0/virtual_size /sys/class/graphics/fb0/bits_per_pixel \
         /sys/class/graphics/fb0/stride /sys/class/graphics/fb0/name; do
    [ -r "$f" ] && printf '%s: %s\n' "$f" "$(cat $f)"
done

sec "Устройства ввода (тачскрин, кнопки)"
try cat /proc/bus/input/devices

sec "Батарея"
for f in capacity status; do
    [ -r /sys/class/power_supply/battery/$f ] && \
        printf '%s: %s\n' "$f" "$(cat /sys/class/power_supply/battery/$f)"
done

sec "Cmdline ядра"
try cat /proc/cmdline

sec "Доступные инструменты"
for t in sh busybox mke2fs mkfs.f2fs e2fsck mount umount dd; do
    printf '%s: %s\n' "$t" "$(command -v $t 2>/dev/null || echo нет)"
done
