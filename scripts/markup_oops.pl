#!/usr/bin/perl -w

use File::Basename;

# Copyright 2008, Intel Corporation
#
# This file is part of the Linux kernel
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# Authors:
# 	Arjan van de Ven <arjan@linux.intel.com>


my $vmlinux_name = $ARGV[0];
if (!defined($vmlinux_name)) {
	my $kerver = `uname -r`;
	chomp($kerver);
	$vmlinux_name = "/lib/modules/$kerver/build/vmlinux";
	print "No vmlinux specified, assuming $vmlinux_name\n";
}
my $filename = $vmlinux_name;
#
# Step 1: Parse the oops to find the EIP value
#

my $target = "0";
my $function;
my $module = "";
my $func_offset;
my $vmaoffset = 0;

my %regs;


sub parse_x86_regs
{
	my ($line) = @_;
	if ($line =~ /EAX: ([0-9a-f]+) EBX: ([0-9a-f]+) ECX: ([0-9a-f]+) EDX: ([0-9a-f]+)/) {
		$regs{"%eax"} = $1;
		$regs{"%ebx"} = $2;
		$regs{"%ecx"} = $3;
		$regs{"%edx"} = $4;
	}
	if ($line =~ /ESI: ([0-9a-f]+) EDI: ([0-9a-f]+) EBP: ([0-9a-f]+) ESP: ([0-9a-f]+)/) {
		$regs{"%esi"} = $1;
		$regs{"%edi"} = $2;
		$regs{"%esp"} = $4;
	}
}

sub process_x86_regs
{
	my ($line, $cntr) = @_;
	my $str = "";
	if (length($line) < 40) {
		return ""; # not an asm istruction
	}

	# find the arguments to the instruction
	if ($line =~ /([0-9a-zA-Z\,\%\(\)\-\+]+)$/) {
		$lastword = $1;
	} else {
		return "";
	}

	# we need to find the registers that get clobbered,
	# since their value is no longer relevant for previous
	# instructions in the stream.

	$clobber = $lastword;
	# first, remove all memory operands, they're read only
	$clobber =~ s/\([a-z0-9\%\,]+\)//g;
	# then, remove everything before the comma, thats the read part
	$clobber =~ s/.*\,//g;

	# if this is the instruction that faulted, we haven't actually done
	# the write yet... nothing is clobbered.
	if ($cntr == 0) {
		$clobber = "";
	}

	foreach $reg (keys(%regs)) {
		my $val = $regs{$reg};
		# first check if we're clobbering this register; if we do
		# we print it with a =>, and then delete its value
		if ($clobber =~ /$reg/) {
			if (length($val) > 0) {
				$str = $str . " $reg => $val ";
			}
			$regs{$reg} = "";
			$val = "";
		}
		# now check if we're reading this register
		if ($lastword =~ /$reg/) {
			if (length($val) > 0) {
				$str = $str . " $reg = $val ";
			}
		}
	}
	return $str;
}

# parse the oops
while (<STDIN>) {
	my $line = $_;
	if ($line =~ /EIP: 0060:\[\<([a-z0-9]+)\>\]/) {
		$target = $1;
	}
	if ($line =~ /EIP is at ([a-zA-Z0-9\_]+)\+(0x[0-9a-f]+)\/0x[a-f0-9]/) {
		$function = $1;
		$func_offset = $2;
	}

	# check if it's a module
	if ($line =~ /EIP is at ([a-zA-Z0-9\_]+)\+(0x[0-9a-f]+)\/0x[a-f0-9]+\W\[([a-zA-Z0-9\_\-]+)\]/) {
		$module = $3;
	}
	parse_x86_regs($line);
}

my $decodestart = hex($target) - hex($func_offset);
my $decodestop = hex($target) + 8192;
if ($target eq "0") {
	print "No oops found!\n";
	print "Usage: \n";
	print "    dmesg | perl scripts/markup_oops.pl vmlinux\n";
	exit;
}

# if it's a module, we need to find the .ko file and calculate a load offset
if ($module ne "") {
	my $dir = dirname($filename);
	$dir = $dir . "/";
	my $mod = $module . ".ko";
	my $modulefile = `find $dir -name $mod | head -1`;
	chomp($modulefile);
	$filename = $modulefile;
	if ($filename eq "") {
		print "Module .ko file for $module not found. Aborting\n";
		exit;
	}
	# ok so we found the module, now we need to calculate the vma offset
	open(FILE, "objdump -dS $filename |") || die "Cannot start objdump";
	while (<FILE>) {
		if ($_ =~ /^([0-9a-f]+) \<$function\>\:/) {
			my $fu = $1;
			$vmaoffset = hex($target) - hex($fu) - hex($func_offset);
		}
	}
	close(FILE);
}

my $counter = 0;
my $state   = 0;
my $center  = 0;
my @lines;
my @reglines;

sub InRange {
	my ($address, $target) = @_;
	my $ad = "0x".$address;
	my $ta = "0x".$target;
	my $delta = hex($ad) - hex($ta);

	if (($delta > -4096) && ($delta < 4096)) {
		return 1;
	}
	return 0;
}



# first, parse the input into the lines array, but to keep size down,
# we only do this for 4Kb around the sweet spot

open(FILE, "objdump -dS --adjust-vma=$vmaoffset --start-address=$decodestart --stop-address=$decodestop $filename |") || die "Cannot start objdump";

while (<FILE>) {
	my $line = $_;
	chomp($line);
	if ($state == 0) {
		if ($line =~ /^([a-f0-9]+)\:/) {
			if (InRange($1, $target)) {
				$state = 1;
			}
		}
	} else {
		if ($line =~ /^([a-f0-9][a-f0-9][a-f0-9][a-f0-9][a-f0-9][a-f0-9]+)\:/) {
			my $val = $1;
			if (!InRange($val, $target)) {
				last;
			}
			if ($val eq $target) {
				$center = $counter;
			}
		}
		$lines[$counter] = $line;

		$counter = $counter + 1;
	}
}

close(FILE);

if ($counter == 0) {
	print "No matching code found \n";
	exit;
}

if ($center == 0) {
	print "No matching code found \n";
	exit;
}

my $start;
my $finish;
my $codelines = 0;
my $binarylines = 0;
# now we go up and down in the array to find how much we want to print

$start = $center;

while ($start > 1) {
	$start = $start - 1;
	my $line = $lines[$start];
	if ($line =~ /^([a-f0-9]+)\:/) {
		$binarylines = $binarylines + 1;
	} else {
		$codelines = $codelines + 1;
	}
	if ($codelines > 10) {
		last;
	}
	if ($binarylines > 20) {
		last;
	}
}


$finish = $center;
$codelines = 0;
$binarylines = 0;
while ($finish < $counter) {
	$finish = $finish + 1;
	my $line = $lines[$finish];
	if ($line =~ /^([a-f0-9]+)\:/) {
		$binarylines = $binarylines + 1;
	} else {
		$codelines = $codelines + 1;
	}
	if ($codelines > 10) {
		last;
	}
	if ($binarylines > 20) {
		last;
	}
}


my $i;


# start annotating the registers in the asm.
# this goes from the oopsing point back, so that the annotator
# can track (opportunistically) which registers got written and
# whos value no longer is relevant.

$i = $center;
while ($i >= $start) {
	$reglines[$i] = process_x86_regs($lines[$i], $center - $i);
	$i = $i - 1;
}

$i = $start;
while ($i < $finish) {
	my $line;
	if ($i == $center) {
		$line =  "*$lines[$i] ";
	} else {
		$line =  " $lines[$i] ";
	}
	print $line;
	if (defined($reglines[$i]) && length($reglines[$i]) > 0) {
		my $c = 60 - length($line);
		while ($c > 0) { print " "; $c = $c - 1; };
		print "| $reglines[$i]";
	}
	if ($i == $center) {
		print "<--- faulting instruction";
	}
	print "\n";
	$i = $i +1;
}

