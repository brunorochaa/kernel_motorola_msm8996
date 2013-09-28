#!/bin/bash
#
# Run a kvm-based test of the specified tree on the specified configs.
# Fully automated run and error checking, no graphics console.
#
# Execute this in the source tree.  Do not run it as a background task
# because qemu does not seem to like that much.
#
# Usage: sh kvm-test-1-rcu.sh config builddir resdir minutes qemu-args bootargs
#
# qemu-args defaults to "" -- you will want "-nographic" if running headless.
# bootargs defaults to	"root=/dev/sda noapic selinux=0 console=ttyS0"
#			"initcall_debug debug rcutorture.stat_interval=15"
#			"rcutorture.shutdown_secs=$((minutes * 60))"
#			"rcutorture.rcutorture_runnable=1"
#
# Anything you specify for either qemu-args or bootargs is appended to
# the default values.  The "-smp" value is deduced from the contents of
# the config fragment.
#
# More sophisticated argument parsing is clearly needed.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# Copyright (C) IBM Corporation, 2011
#
# Authors: Paul E. McKenney <paulmck@linux.vnet.ibm.com>

grace=120

T=/tmp/kvm-test-1-rcu.sh.$$
trap 'rm -rf $T' 0

. $KVM/bin/functions.sh

config_template=${1}
title=`echo $config_template | sed -e 's/^.*\///'`
builddir=${2}
if test -z "$builddir" -o ! -d "$builddir" -o ! -w "$builddir"
then
	echo "kvm-test-1-rcu.sh :$builddir: Not a writable directory, cannot build into it"
	exit 1
fi
resdir=${3}
if test -z "$resdir" -o ! -d "$resdir" -o ! -w "$resdir"
then
	echo "kvm-test-1-rcu.sh :$resdir: Not a writable directory, cannot build into it"
	exit 1
fi
cp $config_template $resdir/ConfigFragment
echo ' ---' `date`: Starting build
cat << '___EOF___' >> $T
CONFIG_RCU_TORTURE_TEST=y
___EOF___
# Optimizations below this point
# CONFIG_USB=n
# CONFIG_SECURITY=n
# CONFIG_NFS_FS=n
# CONFIG_SOUND=n
# CONFIG_INPUT_JOYSTICK=n
# CONFIG_INPUT_TABLET=n
# CONFIG_INPUT_TOUCHSCREEN=n
# CONFIG_INPUT_MISC=n
# CONFIG_INPUT_MOUSE=n
# # CONFIG_NET=n # disables console access, so accept the slower build.
# CONFIG_SCSI=n
# CONFIG_ATA=n
# CONFIG_FAT_FS=n
# CONFIG_MSDOS_FS=n
# CONFIG_VFAT_FS=n
# CONFIG_ISO9660_FS=n
# CONFIG_QUOTA=n
# CONFIG_HID=n
# CONFIG_CRYPTO=n
# CONFIG_PCCARD=n
# CONFIG_PCMCIA=n
# CONFIG_CARDBUS=n
# CONFIG_YENTA=n
if kvm-build.sh $config_template $builddir $T
then
	cp $builddir/Make*.out $resdir
	cp $builddir/.config $resdir
	cp $builddir/arch/x86/boot/bzImage $resdir
	parse-build.sh $resdir/Make.out $title
else
	cp $builddir/Make*.out $resdir
	echo Build failed, not running KVM, see $resdir.
	exit 1
fi
minutes=$4
seconds=$(($minutes * 60))
qemu_args=$5
boot_args=$6

cd $KVM
kstarttime=`awk 'BEGIN { print systime() }' < /dev/null`
echo ' ---' `date`: Starting kernel
if file linux-2.6/*.o | grep -q 64-bit
then
	QEMU=qemu-system-x86_64
else
	QEMU=qemu-system-i386
fi

# Generate -smp qemu argument.
cpu_count=`configNR_CPUS.sh $config_template`
ncpus=`grep '^processor' /proc/cpuinfo | wc -l`
if test $cpu_count -gt $ncpus
then
	echo CPU count limited from $cpu_count to $ncpus
	touch $resdir/Warnings
	echo CPU count limited from $cpu_count to $ncpus >> $resdir/Warnings
	cpu_count=$ncpus
fi
if echo $qemu_args | grep -q -e -smp
then
	echo CPU count specified by caller
else
	qemu_args="$qemu_args -smp $cpu_count"
fi

# Generate CPU-hotplug boot parameters
if ! bootparam_hotplug_cpu "$bootargs"
then
	if configfrag_hotplug_cpu $builddir/.config
	then
		echo Kernel configured for CPU hotplug, adding rcutorture.
		bootargs="$bootargs rcutorture.onoff_interval=3 rcutorture.onoff_holdoff=30"
	fi
fi

echo $QEMU -name rcu-test -serial file:$builddir/console.log $qemu_args -m 512 -kernel $builddir/arch/x86/boot/bzImage -append \"noapic selinux=0 console=ttyS0 initcall_debug debug rcutorture.stat_interval=15 rcutorture.shutdown_secs=$seconds rcutorture.rcutorture_runnable=1 $boot_args\" > $resdir/qemu-cmd
$QEMU -name rcu-test -serial file:$builddir/console.log $qemu_args -m 512 -kernel $builddir/arch/x86/boot/bzImage -append "noapic selinux=0 console=ttyS0 initcall_debug debug rcutorture.stat_interval=15 rcutorture.shutdown_secs=$seconds rcutorture.rcutorture_runnable=1 $boot_args" &
qemu_pid=$!
commandcompleted=0
echo Monitoring qemu job at pid $qemu_pid
for ((i=0;i<$seconds;i++))
do
	if kill -0 $qemu_pid > /dev/null 2>&1
	then
		sleep 1
	else
		commandcompleted=1
		kruntime=`awk 'BEGIN { print systime() - '"$kstarttime"' }' < /dev/null`
		if test $kruntime -lt $seconds
		then
			echo Completed in $kruntime vs. $seconds >> $resdir/Warnings 2>&1
		else
			echo ' ---' `date`: Kernel done
		fi
		break
	fi
done
if test $commandcompleted -eq 0
then
	echo Grace period for qemu job at pid $qemu_pid
	for ((i=0;i<=$grace;i++))
	do
		if kill -0 $qemu_pid > /dev/null 2>&1
		then
			sleep 1
		else
			break
		fi
		if test $i -eq $grace
		then
			kruntime=`awk 'BEGIN { print systime() - '"$kstarttime"' }'`
			echo "!!! Hang at $kruntime vs. $seconds seconds" >> $resdir/Warnings 2>&1
			kill -KILL $qemu_pid
		fi
	done
fi

cp $builddir/console.log $resdir
parse-rcutorture.sh $resdir/console.log $title >> $resdir/Warnings 2>&1
parse-console.sh $resdir/console.log $title >> $resdir/Warnings 2>&1
cat $resdir/Warnings
