#!/bin/bash
#
# Given the results directories for previous KVM runs of rcutorture,
# check the build and console output for errors.  Given a directory
# containing results directories, this recursively checks them all.
#
# Usage: sh kvm-recheck.sh configdir resdir ...
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

PATH=`pwd`/tools/testing/selftests/rcutorture/bin:$PATH; export PATH
configdir=${1}
shift
for rd in "$@"
do
	dirs=`find $rd -name Make.defconfig.out -print | sort | sed -e 's,/[^/]*$,,' | sort -u`
	for i in $dirs
	do
		configfile=`echo $i | sed -e 's/^.*\///'`
		echo $i
		configcheck.sh $i/.config $configdir/$configfile
		parse-build.sh $i/Make.out $configfile
		parse-rcutorture.sh $i/console.log $configfile
		parse-console.sh $i/console.log $configfile
		if test -r $i/Warnings
		then
			cat $i/Warnings
		fi
	done
done
