#!/usr/bin/perl -w
#
# Copyright 2010 - Steven Rostedt <srostedt@redhat.com>, Red Hat Inc.
# Licensed under the terms of the GNU GPL License version 2
#

use strict;
use IPC::Open2;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);
use File::Path qw(mkpath);
use File::Copy qw(cp);
use FileHandle;

my $VERSION = "0.2";

$| = 1;

my %opt;
my %repeat_tests;
my %repeats;
my %default;

#default opts
$default{"NUM_TESTS"}		= 1;
$default{"REBOOT_TYPE"}		= "grub";
$default{"TEST_TYPE"}		= "test";
$default{"BUILD_TYPE"}		= "randconfig";
$default{"MAKE_CMD"}		= "make";
$default{"TIMEOUT"}		= 120;
$default{"TMP_DIR"}		= "/tmp/ktest/\${MACHINE}";
$default{"SLEEP_TIME"}		= 60;	# sleep time between tests
$default{"BUILD_NOCLEAN"}	= 0;
$default{"REBOOT_ON_ERROR"}	= 0;
$default{"POWEROFF_ON_ERROR"}	= 0;
$default{"REBOOT_ON_SUCCESS"}	= 1;
$default{"POWEROFF_ON_SUCCESS"}	= 0;
$default{"BUILD_OPTIONS"}	= "";
$default{"BISECT_SLEEP_TIME"}	= 60;   # sleep time between bisects
$default{"PATCHCHECK_SLEEP_TIME"} = 60; # sleep time between patch checks
$default{"CLEAR_LOG"}		= 0;
$default{"BISECT_MANUAL"}	= 0;
$default{"BISECT_SKIP"}		= 1;
$default{"SUCCESS_LINE"}	= "login:";
$default{"DETECT_TRIPLE_FAULT"} = 1;
$default{"BOOTED_TIMEOUT"}	= 1;
$default{"DIE_ON_FAILURE"}	= 1;
$default{"SSH_EXEC"}		= "ssh \$SSH_USER\@\$MACHINE \$SSH_COMMAND";
$default{"SCP_TO_TARGET"}	= "scp \$SRC_FILE \$SSH_USER\@\$MACHINE:\$DST_FILE";
$default{"REBOOT"}		= "ssh \$SSH_USER\@\$MACHINE reboot";
$default{"STOP_AFTER_SUCCESS"}	= 10;
$default{"STOP_AFTER_FAILURE"}	= 60;
$default{"STOP_TEST_AFTER"}	= 600;
$default{"LOCALVERSION"}	= "-test";

my $ktest_config;
my $version;
my $machine;
my $ssh_user;
my $tmpdir;
my $builddir;
my $outputdir;
my $output_config;
my $test_type;
my $build_type;
my $build_options;
my $pre_build;
my $post_build;
my $pre_build_die;
my $post_build_die;
my $reboot_type;
my $reboot_script;
my $power_cycle;
my $reboot;
my $reboot_on_error;
my $poweroff_on_error;
my $die_on_failure;
my $powercycle_after_reboot;
my $poweroff_after_halt;
my $ssh_exec;
my $scp_to_target;
my $power_off;
my $grub_menu;
my $grub_number;
my $target;
my $make;
my $post_install;
my $noclean;
my $minconfig;
my $start_minconfig;
my $start_minconfig_defined;
my $output_minconfig;
my $ignore_config;
my $addconfig;
my $in_bisect = 0;
my $bisect_bad = "";
my $reverse_bisect;
my $bisect_manual;
my $bisect_skip;
my $config_bisect_good;
my $in_patchcheck = 0;
my $run_test;
my $redirect;
my $buildlog;
my $dmesg;
my $monitor_fp;
my $monitor_pid;
my $monitor_cnt = 0;
my $sleep_time;
my $bisect_sleep_time;
my $patchcheck_sleep_time;
my $ignore_warnings;
my $store_failures;
my $test_name;
my $timeout;
my $booted_timeout;
my $detect_triplefault;
my $console;
my $success_line;
my $stop_after_success;
my $stop_after_failure;
my $stop_test_after;
my $build_target;
my $target_image;
my $localversion;
my $iteration = 0;
my $successes = 0;

my %entered_configs;
my %config_help;
my %variable;
my %force_config;

$config_help{"MACHINE"} = << "EOF"
 The machine hostname that you will test.
EOF
    ;
$config_help{"SSH_USER"} = << "EOF"
 The box is expected to have ssh on normal bootup, provide the user
  (most likely root, since you need privileged operations)
EOF
    ;
$config_help{"BUILD_DIR"} = << "EOF"
 The directory that contains the Linux source code (full path).
EOF
    ;
$config_help{"OUTPUT_DIR"} = << "EOF"
 The directory that the objects will be built (full path).
 (can not be same as BUILD_DIR)
EOF
    ;
$config_help{"BUILD_TARGET"} = << "EOF"
 The location of the compiled file to copy to the target.
 (relative to OUTPUT_DIR)
EOF
    ;
$config_help{"TARGET_IMAGE"} = << "EOF"
 The place to put your image on the test machine.
EOF
    ;
$config_help{"POWER_CYCLE"} = << "EOF"
 A script or command to reboot the box.

 Here is a digital loggers power switch example
 POWER_CYCLE = wget --no-proxy -O /dev/null -q  --auth-no-challenge 'http://admin:admin\@power/outlet?5=CCL'

 Here is an example to reboot a virtual box on the current host
 with the name "Guest".
 POWER_CYCLE = virsh destroy Guest; sleep 5; virsh start Guest
EOF
    ;
$config_help{"CONSOLE"} = << "EOF"
 The script or command that reads the console

  If you use ttywatch server, something like the following would work.
CONSOLE = nc -d localhost 3001

 For a virtual machine with guest name "Guest".
CONSOLE =  virsh console Guest
EOF
    ;
$config_help{"LOCALVERSION"} = << "EOF"
 Required version ending to differentiate the test
 from other linux builds on the system.
EOF
    ;
$config_help{"REBOOT_TYPE"} = << "EOF"
 Way to reboot the box to the test kernel.
 Only valid options so far are "grub" and "script".

 If you specify grub, it will assume grub version 1
 and will search in /boot/grub/menu.lst for the title \$GRUB_MENU
 and select that target to reboot to the kernel. If this is not
 your setup, then specify "script" and have a command or script
 specified in REBOOT_SCRIPT to boot to the target.

 The entry in /boot/grub/menu.lst must be entered in manually.
 The test will not modify that file.
EOF
    ;
$config_help{"GRUB_MENU"} = << "EOF"
 The grub title name for the test kernel to boot
 (Only mandatory if REBOOT_TYPE = grub)

 Note, ktest.pl will not update the grub menu.lst, you need to
 manually add an option for the test. ktest.pl will search
 the grub menu.lst for this option to find what kernel to
 reboot into.

 For example, if in the /boot/grub/menu.lst the test kernel title has:
 title Test Kernel
 kernel vmlinuz-test
 GRUB_MENU = Test Kernel
EOF
    ;
$config_help{"REBOOT_SCRIPT"} = << "EOF"
 A script to reboot the target into the test kernel
 (Only mandatory if REBOOT_TYPE = script)
EOF
    ;

sub read_yn {
    my ($prompt) = @_;

    my $ans;

    for (;;) {
	print "$prompt [Y/n] ";
	$ans = <STDIN>;
	chomp $ans;
	if ($ans =~ /^\s*$/) {
	    $ans = "y";
	}
	last if ($ans =~ /^y$/i || $ans =~ /^n$/i);
	print "Please answer either 'y' or 'n'.\n";
    }
    if ($ans !~ /^y$/i) {
	return 0;
    }
    return 1;
}

sub get_ktest_config {
    my ($config) = @_;

    return if (defined($opt{$config}));

    if (defined($config_help{$config})) {
	print "\n";
	print $config_help{$config};
    }

    for (;;) {
	print "$config = ";
	if (defined($default{$config})) {
	    print "\[$default{$config}\] ";
	}
	$entered_configs{$config} = <STDIN>;
	$entered_configs{$config} =~ s/^\s*(.*\S)\s*$/$1/;
	if ($entered_configs{$config} =~ /^\s*$/) {
	    if ($default{$config}) {
		$entered_configs{$config} = $default{$config};
	    } else {
		print "Your answer can not be blank\n";
		next;
	    }
	}
	last;
    }
}

sub get_ktest_configs {
    get_ktest_config("MACHINE");
    get_ktest_config("SSH_USER");
    get_ktest_config("BUILD_DIR");
    get_ktest_config("OUTPUT_DIR");
    get_ktest_config("BUILD_TARGET");
    get_ktest_config("TARGET_IMAGE");
    get_ktest_config("POWER_CYCLE");
    get_ktest_config("CONSOLE");
    get_ktest_config("LOCALVERSION");

    my $rtype = $opt{"REBOOT_TYPE"};

    if (!defined($rtype)) {
	if (!defined($opt{"GRUB_MENU"})) {
	    get_ktest_config("REBOOT_TYPE");
	    $rtype = $entered_configs{"REBOOT_TYPE"};
	} else {
	    $rtype = "grub";
	}
    }

    if ($rtype eq "grub") {
	get_ktest_config("GRUB_MENU");
    } else {
	get_ktest_config("REBOOT_SCRIPT");
    }
}

sub process_variables {
    my ($value) = @_;
    my $retval = "";

    # We want to check for '\', and it is just easier
    # to check the previous characet of '$' and not need
    # to worry if '$' is the first character. By adding
    # a space to $value, we can just check [^\\]\$ and
    # it will still work.
    $value = " $value";

    while ($value =~ /(.*?[^\\])\$\{(.*?)\}(.*)/) {
	my $begin = $1;
	my $var = $2;
	my $end = $3;
	# append beginning of value to retval
	$retval = "$retval$begin";
	if (defined($variable{$var})) {
	    $retval = "$retval$variable{$var}";
	} else {
	    # put back the origin piece.
	    $retval = "$retval\$\{$var\}";
	}
	$value = $end;
    }
    $retval = "$retval$value";

    # remove the space added in the beginning
    $retval =~ s/ //;

    return "$retval"
}

sub set_value {
    my ($lvalue, $rvalue) = @_;

    if (defined($opt{$lvalue})) {
	die "Error: Option $lvalue defined more than once!\n";
    }
    if ($rvalue =~ /^\s*$/) {
	delete $opt{$lvalue};
    } else {
	$rvalue = process_variables($rvalue);
	$opt{$lvalue} = $rvalue;
    }
}

sub set_variable {
    my ($lvalue, $rvalue) = @_;

    if ($rvalue =~ /^\s*$/) {
	delete $variable{$lvalue};
    } else {
	$rvalue = process_variables($rvalue);
	$variable{$lvalue} = $rvalue;
    }
}

sub read_config {
    my ($config) = @_;

    open(IN, $config) || die "can't read file $config";

    my $name = $config;
    $name =~ s,.*/(.*),$1,;

    my $test_num = 0;
    my $default = 1;
    my $repeat = 1;
    my $num_tests_set = 0;
    my $skip = 0;
    my $rest;
    my $test_case = 0;

    while (<IN>) {

	# ignore blank lines and comments
	next if (/^\s*$/ || /\s*\#/);

	if (/^\s*TEST_START(.*)/) {

	    $rest = $1;

	    if ($num_tests_set) {
		die "$name: $.: Can not specify both NUM_TESTS and TEST_START\n";
	    }

	    my $old_test_num = $test_num;
	    my $old_repeat = $repeat;

	    $test_num += $repeat;
	    $default = 0;
	    $repeat = 1;

	    if ($rest =~ /\s+SKIP(.*)/) {
		$rest = $1;
		$skip = 1;
	    } else {
		$test_case = 1;
		$skip = 0;
	    }

	    if ($rest =~ /\s+ITERATE\s+(\d+)(.*)$/) {
		$repeat = $1;
		$rest = $2;
		$repeat_tests{"$test_num"} = $repeat;
	    }

	    if ($rest =~ /\s+SKIP(.*)/) {
		$rest = $1;
		$skip = 1;
	    }

	    if ($rest !~ /^\s*$/) {
		die "$name: $.: Gargbage found after TEST_START\n$_";
	    }

	    if ($skip) {
		$test_num = $old_test_num;
		$repeat = $old_repeat;
	    }

	} elsif (/^\s*DEFAULTS(.*)$/) {
	    $default = 1;

	    $rest = $1;

	    if ($rest =~ /\s+SKIP(.*)/) {
		$rest = $1;
		$skip = 1;
	    } else {
		$skip = 0;
	    }

	    if ($rest !~ /^\s*$/) {
		die "$name: $.: Gargbage found after DEFAULTS\n$_";
	    }

	} elsif (/^\s*([A-Z_\[\]\d]+)\s*=\s*(.*?)\s*$/) {

	    next if ($skip);

	    my $lvalue = $1;
	    my $rvalue = $2;

	    if (!$default &&
		($lvalue eq "NUM_TESTS" ||
		 $lvalue eq "LOG_FILE" ||
		 $lvalue eq "CLEAR_LOG")) {
		die "$name: $.: $lvalue must be set in DEFAULTS section\n";
	    }

	    if ($lvalue eq "NUM_TESTS") {
		if ($test_num) {
		    die "$name: $.: Can not specify both NUM_TESTS and TEST_START\n";
		}
		if (!$default) {
		    die "$name: $.: NUM_TESTS must be set in default section\n";
		}
		$num_tests_set = 1;
	    }

	    if ($default || $lvalue =~ /\[\d+\]$/) {
		set_value($lvalue, $rvalue);
	    } else {
		my $val = "$lvalue\[$test_num\]";
		set_value($val, $rvalue);

		if ($repeat > 1) {
		    $repeats{$val} = $repeat;
		}
	    }
	} elsif (/^\s*([A-Z_\[\]\d]+)\s*:=\s*(.*?)\s*$/) {
	    next if ($skip);

	    my $lvalue = $1;
	    my $rvalue = $2;

	    # process config variables.
	    # Config variables are only active while reading the
	    # config and can be defined anywhere. They also ignore
	    # TEST_START and DEFAULTS, but are skipped if they are in
	    # on of these sections that have SKIP defined.
	    # The save variable can be
	    # defined multiple times and the new one simply overrides
	    # the prevous one.
	    set_variable($lvalue, $rvalue);

	} else {
	    die "$name: $.: Garbage found in config\n$_";
	}
    }

    close(IN);

    if ($test_num) {
	$test_num += $repeat - 1;
	$opt{"NUM_TESTS"} = $test_num;
    }

    # make sure we have all mandatory configs
    get_ktest_configs;

    # was a test specified?
    if (!$test_case) {
	print "No test case specified.\n";
	print "What test case would you like to run?\n";
	my $ans = <STDIN>;
	chomp $ans;
	$default{"TEST_TYPE"} = $ans;
    }

    # set any defaults

    foreach my $default (keys %default) {
	if (!defined($opt{$default})) {
	    $opt{$default} = $default{$default};
	}
    }
}

sub __eval_option {
    my ($option, $i) = @_;

    # Add space to evaluate the character before $
    $option = " $option";
    my $retval = "";

    while ($option =~ /(.*?[^\\])\$\{(.*?)\}(.*)/) {
	my $start = $1;
	my $var = $2;
	my $end = $3;

	# Append beginning of line
	$retval = "$retval$start";

	# If the iteration option OPT[$i] exists, then use that.
	# otherwise see if the default OPT (without [$i]) exists.

	my $o = "$var\[$i\]";

	if (defined($opt{$o})) {
	    $o = $opt{$o};
	    $retval = "$retval$o";
	} elsif (defined($opt{$var})) {
	    $o = $opt{$var};
	    $retval = "$retval$o";
	} else {
	    $retval = "$retval\$\{$var\}";
	}

	$option = $end;
    }

    $retval = "$retval$option";

    $retval =~ s/^ //;

    return $retval;
}

sub eval_option {
    my ($option, $i) = @_;

    my $prev = "";

    # Since an option can evaluate to another option,
    # keep iterating until we do not evaluate any more
    # options.
    my $r = 0;
    while ($prev ne $option) {
	# Check for recursive evaluations.
	# 100 deep should be more than enough.
	if ($r++ > 100) {
	    die "Over 100 evaluations accurred with $option\n" .
		"Check for recursive variables\n";
	}
	$prev = $option;
	$option = __eval_option($option, $i);
    }

    return $option;
}

sub _logit {
    if (defined($opt{"LOG_FILE"})) {
	open(OUT, ">> $opt{LOG_FILE}") or die "Can't write to $opt{LOG_FILE}";
	print OUT @_;
	close(OUT);
    }
}

sub logit {
    if (defined($opt{"LOG_FILE"})) {
	_logit @_;
    } else {
	print @_;
    }
}

sub doprint {
    print @_;
    _logit @_;
}

sub run_command;
sub start_monitor;
sub end_monitor;
sub wait_for_monitor;

sub reboot {
    my ($time) = @_;

    # try to reboot normally
    if (run_command $reboot) {
	if (defined($powercycle_after_reboot)) {
	    sleep $powercycle_after_reboot;
	    run_command "$power_cycle";
	}
    } else {
	# nope? power cycle it.
	run_command "$power_cycle";
    }

    if (defined($time)) {
	start_monitor;
	wait_for_monitor $time;
	end_monitor;
    }
}

sub do_not_reboot {
    my $i = $iteration;

    return $test_type eq "build" ||
	($test_type eq "patchcheck" && $opt{"PATCHCHECK_TYPE[$i]"} eq "build") ||
	($test_type eq "bisect" && $opt{"BISECT_TYPE[$i]"} eq "build");
}

sub dodie {
    doprint "CRITICAL FAILURE... ", @_, "\n";

    my $i = $iteration;

    if ($reboot_on_error && !do_not_reboot) {

	doprint "REBOOTING\n";
	reboot;

    } elsif ($poweroff_on_error && defined($power_off)) {
	doprint "POWERING OFF\n";
	`$power_off`;
    }

    if (defined($opt{"LOG_FILE"})) {
	print " See $opt{LOG_FILE} for more info.\n";
    }

    die @_, "\n";
}

sub open_console {
    my ($fp) = @_;

    my $flags;

    my $pid = open($fp, "$console|") or
	dodie "Can't open console $console";

    $flags = fcntl($fp, F_GETFL, 0) or
	dodie "Can't get flags for the socket: $!";
    $flags = fcntl($fp, F_SETFL, $flags | O_NONBLOCK) or
	dodie "Can't set flags for the socket: $!";

    return $pid;
}

sub close_console {
    my ($fp, $pid) = @_;

    doprint "kill child process $pid\n";
    kill 2, $pid;

    print "closing!\n";
    close($fp);
}

sub start_monitor {
    if ($monitor_cnt++) {
	return;
    }
    $monitor_fp = \*MONFD;
    $monitor_pid = open_console $monitor_fp;

    return;

    open(MONFD, "Stop perl from warning about single use of MONFD");
}

sub end_monitor {
    if (--$monitor_cnt) {
	return;
    }
    close_console($monitor_fp, $monitor_pid);
}

sub wait_for_monitor {
    my ($time) = @_;
    my $line;

    doprint "** Wait for monitor to settle down **\n";

    # read the monitor and wait for the system to calm down
    do {
	$line = wait_for_input($monitor_fp, $time);
	print "$line" if (defined($line));
    } while (defined($line));
    print "** Monitor flushed **\n";
}

sub fail {

	if ($die_on_failure) {
		dodie @_;
	}

	doprint "FAILED\n";

	my $i = $iteration;

	# no need to reboot for just building.
	if (!do_not_reboot) {
	    doprint "REBOOTING\n";
	    reboot $sleep_time;
	}

	my $name = "";

	if (defined($test_name)) {
	    $name = " ($test_name)";
	}

	doprint "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n";
	doprint "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n";
	doprint "KTEST RESULT: TEST $i$name Failed: ", @_, "\n";
	doprint "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n";
	doprint "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n";

	return 1 if (!defined($store_failures));

	my @t = localtime;
	my $date = sprintf "%04d%02d%02d%02d%02d%02d",
		1900+$t[5],$t[4],$t[3],$t[2],$t[1],$t[0];

	my $type = $build_type;
	if ($type =~ /useconfig/) {
	    $type = "useconfig";
	}

	my $dir = "$machine-$test_type-$type-fail-$date";
	my $faildir = "$store_failures/$dir";

	if (!-d $faildir) {
	    mkpath($faildir) or
		die "can't create $faildir";
	}
	if (-f "$output_config") {
	    cp "$output_config", "$faildir/config" or
		die "failed to copy .config";
	}
	if (-f $buildlog) {
	    cp $buildlog, "$faildir/buildlog" or
		die "failed to move $buildlog";
	}
	if (-f $dmesg) {
	    cp $dmesg, "$faildir/dmesg" or
		die "failed to move $dmesg";
	}

	doprint "*** Saved info to $faildir ***\n";

	return 1;
}

sub run_command {
    my ($command) = @_;
    my $dolog = 0;
    my $dord = 0;
    my $pid;

    $command =~ s/\$SSH_USER/$ssh_user/g;
    $command =~ s/\$MACHINE/$machine/g;

    doprint("$command ... ");

    $pid = open(CMD, "$command 2>&1 |") or
	(fail "unable to exec $command" and return 0);

    if (defined($opt{"LOG_FILE"})) {
	open(LOG, ">>$opt{LOG_FILE}") or
	    dodie "failed to write to log";
	$dolog = 1;
    }

    if (defined($redirect)) {
	open (RD, ">$redirect") or
	    dodie "failed to write to redirect $redirect";
	$dord = 1;
    }

    while (<CMD>) {
	print LOG if ($dolog);
	print RD  if ($dord);
    }

    waitpid($pid, 0);
    my $failed = $?;

    close(CMD);
    close(LOG) if ($dolog);
    close(RD)  if ($dord);

    if ($failed) {
	doprint "FAILED!\n";
    } else {
	doprint "SUCCESS\n";
    }

    return !$failed;
}

sub run_ssh {
    my ($cmd) = @_;
    my $cp_exec = $ssh_exec;

    $cp_exec =~ s/\$SSH_COMMAND/$cmd/g;
    return run_command "$cp_exec";
}

sub run_scp {
    my ($src, $dst) = @_;
    my $cp_scp = $scp_to_target;

    $cp_scp =~ s/\$SRC_FILE/$src/g;
    $cp_scp =~ s/\$DST_FILE/$dst/g;

    return run_command "$cp_scp";
}

sub get_grub_index {

    if ($reboot_type ne "grub") {
	return;
    }
    return if (defined($grub_number));

    doprint "Find grub menu ... ";
    $grub_number = -1;

    my $ssh_grub = $ssh_exec;
    $ssh_grub =~ s,\$SSH_COMMAND,cat /boot/grub/menu.lst,g;

    open(IN, "$ssh_grub |")
	or die "unable to get menu.lst";

    while (<IN>) {
	if (/^\s*title\s+$grub_menu\s*$/) {
	    $grub_number++;
	    last;
	} elsif (/^\s*title\s/) {
	    $grub_number++;
	}
    }
    close(IN);

    die "Could not find '$grub_menu' in /boot/grub/menu on $machine"
	if ($grub_number < 0);
    doprint "$grub_number\n";
}

sub wait_for_input
{
    my ($fp, $time) = @_;
    my $rin;
    my $ready;
    my $line;
    my $ch;

    if (!defined($time)) {
	$time = $timeout;
    }

    $rin = '';
    vec($rin, fileno($fp), 1) = 1;
    $ready = select($rin, undef, undef, $time);

    $line = "";

    # try to read one char at a time
    while (sysread $fp, $ch, 1) {
	$line .= $ch;
	last if ($ch eq "\n");
    }

    if (!length($line)) {
	return undef;
    }

    return $line;
}

sub reboot_to {
    if ($reboot_type eq "grub") {
	run_ssh "'(echo \"savedefault --default=$grub_number --once\" | grub --batch && reboot)'";
	return;
    }

    run_command "$reboot_script";
}

sub get_sha1 {
    my ($commit) = @_;

    doprint "git rev-list --max-count=1 $commit ... ";
    my $sha1 = `git rev-list --max-count=1 $commit`;
    my $ret = $?;

    logit $sha1;

    if ($ret) {
	doprint "FAILED\n";
	dodie "Failed to get git $commit";
    }

    print "SUCCESS\n";

    chomp $sha1;

    return $sha1;
}

sub monitor {
    my $booted = 0;
    my $bug = 0;
    my $skip_call_trace = 0;
    my $loops;

    wait_for_monitor 5;

    my $line;
    my $full_line = "";

    open(DMESG, "> $dmesg") or
	die "unable to write to $dmesg";

    reboot_to;

    my $success_start;
    my $failure_start;
    my $monitor_start = time;
    my $done = 0;
    my $version_found = 0;

    while (!$done) {

	if ($bug && defined($stop_after_failure) &&
	    $stop_after_failure >= 0) {
	    my $time = $stop_after_failure - (time - $failure_start);
	    $line = wait_for_input($monitor_fp, $time);
	    if (!defined($line)) {
		doprint "bug timed out after $booted_timeout seconds\n";
		doprint "Test forced to stop after $stop_after_failure seconds after failure\n";
		last;
	    }
	} elsif ($booted) {
	    $line = wait_for_input($monitor_fp, $booted_timeout);
	    if (!defined($line)) {
		my $s = $booted_timeout == 1 ? "" : "s";
		doprint "Successful boot found: break after $booted_timeout second$s\n";
		last;
	    }
	} else {
	    $line = wait_for_input($monitor_fp);
	    if (!defined($line)) {
		my $s = $timeout == 1 ? "" : "s";
		doprint "Timed out after $timeout second$s\n";
		last;
	    }
	}

	doprint $line;
	print DMESG $line;

	# we are not guaranteed to get a full line
	$full_line .= $line;

	if ($full_line =~ /$success_line/) {
	    $booted = 1;
	    $success_start = time;
	}

	if ($booted && defined($stop_after_success) &&
	    $stop_after_success >= 0) {
	    my $now = time;
	    if ($now - $success_start >= $stop_after_success) {
		doprint "Test forced to stop after $stop_after_success seconds after success\n";
		last;
	    }
	}

	if ($full_line =~ /\[ backtrace testing \]/) {
	    $skip_call_trace = 1;
	}

	if ($full_line =~ /call trace:/i) {
	    if (!$bug && !$skip_call_trace) {
		$bug = 1;
		$failure_start = time;
	    }
	}

	if ($bug && defined($stop_after_failure) &&
	    $stop_after_failure >= 0) {
	    my $now = time;
	    if ($now - $failure_start >= $stop_after_failure) {
		doprint "Test forced to stop after $stop_after_failure seconds after failure\n";
		last;
	    }
	}

	if ($full_line =~ /\[ end of backtrace testing \]/) {
	    $skip_call_trace = 0;
	}

	if ($full_line =~ /Kernel panic -/) {
	    $failure_start = time;
	    $bug = 1;
	}

	# Detect triple faults by testing the banner
	if ($full_line =~ /\bLinux version (\S+).*\n/) {
	    if ($1 eq $version) {
		$version_found = 1;
	    } elsif ($version_found && $detect_triplefault) {
		# We already booted into the kernel we are testing,
		# but now we booted into another kernel?
		# Consider this a triple fault.
		doprint "Aleady booted in Linux kernel $version, but now\n";
		doprint "we booted into Linux kernel $1.\n";
		doprint "Assuming that this is a triple fault.\n";
		doprint "To disable this: set DETECT_TRIPLE_FAULT to 0\n";
		last;
	    }
	}

	if ($line =~ /\n/) {
	    $full_line = "";
	}

	if ($stop_test_after > 0 && !$booted && !$bug) {
	    if (time - $monitor_start > $stop_test_after) {
		doprint "STOP_TEST_AFTER ($stop_test_after seconds) timed out\n";
		$done = 1;
	    }
	}
    }

    close(DMESG);

    if ($bug) {
	return 0 if ($in_bisect);
	fail "failed - got a bug report" and return 0;
    }

    if (!$booted) {
	return 0 if ($in_bisect);
	fail "failed - never got a boot prompt." and return 0;
    }

    return 1;
}

sub do_post_install {

    return if (!defined($post_install));

    my $cp_post_install = $post_install;
    $cp_post_install =~ s/\$KERNEL_VERSION/$version/g;
    run_command "$cp_post_install" or
	dodie "Failed to run post install";
}

sub install {

    run_scp "$outputdir/$build_target", "$target_image" or
	dodie "failed to copy image";

    my $install_mods = 0;

    # should we process modules?
    $install_mods = 0;
    open(IN, "$output_config") or dodie("Can't read config file");
    while (<IN>) {
	if (/CONFIG_MODULES(=y)?/) {
	    $install_mods = 1 if (defined($1));
	    last;
	}
    }
    close(IN);

    if (!$install_mods) {
	do_post_install;
	doprint "No modules needed\n";
	return;
    }

    run_command "$make INSTALL_MOD_PATH=$tmpdir modules_install" or
	dodie "Failed to install modules";

    my $modlib = "/lib/modules/$version";
    my $modtar = "ktest-mods.tar.bz2";

    run_ssh "rm -rf $modlib" or
	dodie "failed to remove old mods: $modlib";

    # would be nice if scp -r did not follow symbolic links
    run_command "cd $tmpdir && tar -cjf $modtar lib/modules/$version" or
	dodie "making tarball";

    run_scp "$tmpdir/$modtar", "/tmp" or
	dodie "failed to copy modules";

    unlink "$tmpdir/$modtar";

    run_ssh "'(cd / && tar xjf /tmp/$modtar)'" or
	dodie "failed to tar modules";

    run_ssh "rm -f /tmp/$modtar";

    do_post_install;
}

sub get_version {
    # get the release name
    doprint "$make kernelrelease ... ";
    $version = `$make kernelrelease | tail -1`;
    chomp($version);
    doprint "$version\n";
}

sub start_monitor_and_boot {
    get_grub_index;
    get_version;
    install;

    start_monitor;
    return monitor;
}

sub check_buildlog {
    my ($patch) = @_;

    my @files = `git show $patch | diffstat -l`;

    open(IN, "git show $patch |") or
	dodie "failed to show $patch";
    while (<IN>) {
	if (m,^--- a/(.*),) {
	    chomp $1;
	    $files[$#files] = $1;
	}
    }
    close(IN);

    open(IN, $buildlog) or dodie "Can't open $buildlog";
    while (<IN>) {
	if (/^\s*(.*?):.*(warning|error)/) {
	    my $err = $1;
	    foreach my $file (@files) {
		my $fullpath = "$builddir/$file";
		if ($file eq $err || $fullpath eq $err) {
		    fail "$file built with warnings" and return 0;
		}
	    }
	}
    }
    close(IN);

    return 1;
}

sub apply_min_config {
    my $outconfig = "$output_config.new";

    # Read the config file and remove anything that
    # is in the force_config hash (from minconfig and others)
    # then add the force config back.

    doprint "Applying minimum configurations into $output_config.new\n";

    open (OUT, ">$outconfig") or
	dodie "Can't create $outconfig";

    if (-f $output_config) {
	open (IN, $output_config) or
	    dodie "Failed to open $output_config";
	while (<IN>) {
	    if (/^(# )?(CONFIG_[^\s=]*)/) {
		next if (defined($force_config{$2}));
	    }
	    print OUT;
	}
	close IN;
    }
    foreach my $config (keys %force_config) {
	print OUT "$force_config{$config}\n";
    }
    close OUT;

    run_command "mv $outconfig $output_config";
}

sub make_oldconfig {

    my @force_list = keys %force_config;

    if ($#force_list >= 0) {
	apply_min_config;
    }

    if (!run_command "$make oldnoconfig") {
	# Perhaps oldnoconfig doesn't exist in this version of the kernel
	# try a yes '' | oldconfig
	doprint "oldnoconfig failed, trying yes '' | make oldconfig\n";
	run_command "yes '' | $make oldconfig" or
	    dodie "failed make config oldconfig";
    }
}

# read a config file and use this to force new configs.
sub load_force_config {
    my ($config) = @_;

    open(IN, $config) or
	dodie "failed to read $config";
    while (<IN>) {
	chomp;
	if (/^(CONFIG[^\s=]*)(\s*=.*)/) {
	    $force_config{$1} = $_;
	} elsif (/^# (CONFIG_\S*) is not set/) {
	    $force_config{$1} = $_;
	}
    }
    close IN;
}

sub build {
    my ($type) = @_;

    unlink $buildlog;

    if (defined($pre_build)) {
	my $ret = run_command $pre_build;
	if (!$ret && defined($pre_build_die) &&
	    $pre_build_die) {
	    dodie "failed to pre_build\n";
	}
    }

    if ($type =~ /^useconfig:(.*)/) {
	run_command "cp $1 $output_config" or
	    dodie "could not copy $1 to .config";

	$type = "oldconfig";
    }

    # old config can ask questions
    if ($type eq "oldconfig") {
	$type = "oldnoconfig";

	# allow for empty configs
	run_command "touch $output_config";

	if (!$noclean) {
	    run_command "mv $output_config $outputdir/config_temp" or
		dodie "moving .config";

	    run_command "$make mrproper" or dodie "make mrproper";

	    run_command "mv $outputdir/config_temp $output_config" or
		dodie "moving config_temp";
	}

    } elsif (!$noclean) {
	unlink "$output_config";
	run_command "$make mrproper" or
	    dodie "make mrproper";
    }

    # add something to distinguish this build
    open(OUT, "> $outputdir/localversion") or dodie("Can't make localversion file");
    print OUT "$localversion\n";
    close(OUT);

    if (defined($minconfig)) {
	load_force_config($minconfig);
    }

    if ($type ne "oldnoconfig") {
	run_command "$make $type" or
	    dodie "failed make config";
    }
    # Run old config regardless, to enforce min configurations
    make_oldconfig;

    $redirect = "$buildlog";
    my $build_ret = run_command "$make $build_options";
    undef $redirect;

    if (defined($post_build)) {
	my $ret = run_command $post_build;
	if (!$ret && defined($post_build_die) &&
	    $post_build_die) {
	    dodie "failed to post_build\n";
	}
    }

    if (!$build_ret) {
	# bisect may need this to pass
	return 0 if ($in_bisect);
	fail "failed build" and return 0;
    }

    return 1;
}

sub halt {
    if (!run_ssh "halt" or defined($power_off)) {
	if (defined($poweroff_after_halt)) {
	    sleep $poweroff_after_halt;
	    run_command "$power_off";
	}
    } else {
	# nope? the zap it!
	run_command "$power_off";
    }
}

sub success {
    my ($i) = @_;

    $successes++;

    my $name = "";

    if (defined($test_name)) {
	$name = " ($test_name)";
    }

    doprint "\n\n*******************************************\n";
    doprint     "*******************************************\n";
    doprint     "KTEST RESULT: TEST $i$name SUCCESS!!!!         **\n";
    doprint     "*******************************************\n";
    doprint     "*******************************************\n";

    if ($i != $opt{"NUM_TESTS"} && !do_not_reboot) {
	doprint "Reboot and wait $sleep_time seconds\n";
	reboot $sleep_time;
    }
}

sub answer_bisect {
    for (;;) {
	doprint "Pass or fail? [p/f]";
	my $ans = <STDIN>;
	chomp $ans;
	if ($ans eq "p" || $ans eq "P") {
	    return 1;
	} elsif ($ans eq "f" || $ans eq "F") {
	    return 0;
	} else {
	    print "Please answer 'P' or 'F'\n";
	}
    }
}

sub child_run_test {
    my $failed = 0;

    # child should have no power
    $reboot_on_error = 0;
    $poweroff_on_error = 0;
    $die_on_failure = 1;

    run_command $run_test or $failed = 1;
    exit $failed;
}

my $child_done;

sub child_finished {
    $child_done = 1;
}

sub do_run_test {
    my $child_pid;
    my $child_exit;
    my $line;
    my $full_line;
    my $bug = 0;

    wait_for_monitor 1;

    doprint "run test $run_test\n";

    $child_done = 0;

    $SIG{CHLD} = qw(child_finished);

    $child_pid = fork;

    child_run_test if (!$child_pid);

    $full_line = "";

    do {
	$line = wait_for_input($monitor_fp, 1);
	if (defined($line)) {

	    # we are not guaranteed to get a full line
	    $full_line .= $line;
	    doprint $line;

	    if ($full_line =~ /call trace:/i) {
		$bug = 1;
	    }

	    if ($full_line =~ /Kernel panic -/) {
		$bug = 1;
	    }

	    if ($line =~ /\n/) {
		$full_line = "";
	    }
	}
    } while (!$child_done && !$bug);

    if ($bug) {
	my $failure_start = time;
	my $now;
	do {
	    $line = wait_for_input($monitor_fp, 1);
	    if (defined($line)) {
		doprint $line;
	    }
	    $now = time;
	    if ($now - $failure_start >= $stop_after_failure) {
		last;
	    }
	} while (defined($line));

	doprint "Detected kernel crash!\n";
	# kill the child with extreme prejudice
	kill 9, $child_pid;
    }

    waitpid $child_pid, 0;
    $child_exit = $?;

    if ($bug || $child_exit) {
	return 0 if $in_bisect;
	fail "test failed" and return 0;
    }
    return 1;
}

sub run_git_bisect {
    my ($command) = @_;

    doprint "$command ... ";

    my $output = `$command 2>&1`;
    my $ret = $?;

    logit $output;

    if ($ret) {
	doprint "FAILED\n";
	dodie "Failed to git bisect";
    }

    doprint "SUCCESS\n";
    if ($output =~ m/^(Bisecting: .*\(roughly \d+ steps?\))\s+\[([[:xdigit:]]+)\]/) {
	doprint "$1 [$2]\n";
    } elsif ($output =~ m/^([[:xdigit:]]+) is the first bad commit/) {
	$bisect_bad = $1;
	doprint "Found bad commit... $1\n";
	return 0;
    } else {
	# we already logged it, just print it now.
	print $output;
    }

    return 1;
}

sub bisect_reboot {
    doprint "Reboot and sleep $bisect_sleep_time seconds\n";
    reboot $bisect_sleep_time;
}

# returns 1 on success, 0 on failure, -1 on skip
sub run_bisect_test {
    my ($type, $buildtype) = @_;

    my $failed = 0;
    my $result;
    my $output;
    my $ret;

    $in_bisect = 1;

    build $buildtype or $failed = 1;

    if ($type ne "build") {
	if ($failed && $bisect_skip) {
	    $in_bisect = 0;
	    return -1;
	}
	dodie "Failed on build" if $failed;

	# Now boot the box
	start_monitor_and_boot or $failed = 1;

	if ($type ne "boot") {
	    if ($failed && $bisect_skip) {
		end_monitor;
		bisect_reboot;
		$in_bisect = 0;
		return -1;
	    }
	    dodie "Failed on boot" if $failed;

	    do_run_test or $failed = 1;
	}
	end_monitor;
    }

    if ($failed) {
	$result = 0;
    } else {
	$result = 1;
    }

    # reboot the box to a kernel we can ssh to
    if ($type ne "build") {
	bisect_reboot;
    }
    $in_bisect = 0;

    return $result;
}

sub run_bisect {
    my ($type) = @_;
    my $buildtype = "oldconfig";

    # We should have a minconfig to use?
    if (defined($minconfig)) {
	$buildtype = "useconfig:$minconfig";
    }

    my $ret = run_bisect_test $type, $buildtype;

    if ($bisect_manual) {
	$ret = answer_bisect;
    }

    # Are we looking for where it worked, not failed?
    if ($reverse_bisect) {
	$ret = !$ret;
    }

    if ($ret > 0) {
	return "good";
    } elsif ($ret == 0) {
	return  "bad";
    } elsif ($bisect_skip) {
	doprint "HIT A BAD COMMIT ... SKIPPING\n";
	return "skip";
    }
}

sub bisect {
    my ($i) = @_;

    my $result;

    die "BISECT_GOOD[$i] not defined\n"	if (!defined($opt{"BISECT_GOOD[$i]"}));
    die "BISECT_BAD[$i] not defined\n"	if (!defined($opt{"BISECT_BAD[$i]"}));
    die "BISECT_TYPE[$i] not defined\n"	if (!defined($opt{"BISECT_TYPE[$i]"}));

    my $good = $opt{"BISECT_GOOD[$i]"};
    my $bad = $opt{"BISECT_BAD[$i]"};
    my $type = $opt{"BISECT_TYPE[$i]"};
    my $start = $opt{"BISECT_START[$i]"};
    my $replay = $opt{"BISECT_REPLAY[$i]"};
    my $start_files = $opt{"BISECT_FILES[$i]"};

    if (defined($start_files)) {
	$start_files = " -- " . $start_files;
    } else {
	$start_files = "";
    }

    # convert to true sha1's
    $good = get_sha1($good);
    $bad = get_sha1($bad);

    if (defined($opt{"BISECT_REVERSE[$i]"}) &&
	$opt{"BISECT_REVERSE[$i]"} == 1) {
	doprint "Performing a reverse bisect (bad is good, good is bad!)\n";
	$reverse_bisect = 1;
    } else {
	$reverse_bisect = 0;
    }

    # Can't have a test without having a test to run
    if ($type eq "test" && !defined($run_test)) {
	$type = "boot";
    }

    my $check = $opt{"BISECT_CHECK[$i]"};
    if (defined($check) && $check ne "0") {

	# get current HEAD
	my $head = get_sha1("HEAD");

	if ($check ne "good") {
	    doprint "TESTING BISECT BAD [$bad]\n";
	    run_command "git checkout $bad" or
		die "Failed to checkout $bad";

	    $result = run_bisect $type;

	    if ($result ne "bad") {
		fail "Tested BISECT_BAD [$bad] and it succeeded" and return 0;
	    }
	}

	if ($check ne "bad") {
	    doprint "TESTING BISECT GOOD [$good]\n";
	    run_command "git checkout $good" or
		die "Failed to checkout $good";

	    $result = run_bisect $type;

	    if ($result ne "good") {
		fail "Tested BISECT_GOOD [$good] and it failed" and return 0;
	    }
	}

	# checkout where we started
	run_command "git checkout $head" or
	    die "Failed to checkout $head";
    }

    run_command "git bisect start$start_files" or
	dodie "could not start bisect";

    run_command "git bisect good $good" or
	dodie "could not set bisect good to $good";

    run_git_bisect "git bisect bad $bad" or
	dodie "could not set bisect bad to $bad";

    if (defined($replay)) {
	run_command "git bisect replay $replay" or
	    dodie "failed to run replay";
    }

    if (defined($start)) {
	run_command "git checkout $start" or
	    dodie "failed to checkout $start";
    }

    my $test;
    do {
	$result = run_bisect $type;
	$test = run_git_bisect "git bisect $result";
    } while ($test);

    run_command "git bisect log" or
	dodie "could not capture git bisect log";

    run_command "git bisect reset" or
	dodie "could not reset git bisect";

    doprint "Bad commit was [$bisect_bad]\n";

    success $i;
}

my %config_ignore;
my %config_set;

my %config_list;
my %null_config;

my %dependency;

sub assign_configs {
    my ($hash, $config) = @_;

    open (IN, $config)
	or dodie "Failed to read $config";

    while (<IN>) {
	if (/^((CONFIG\S*)=.*)/) {
	    ${$hash}{$2} = $1;
	}
    }

    close(IN);
}

sub process_config_ignore {
    my ($config) = @_;

    assign_configs \%config_ignore, $config;
}

sub read_current_config {
    my ($config_ref) = @_;

    %{$config_ref} = ();
    undef %{$config_ref};

    my @key = keys %{$config_ref};
    if ($#key >= 0) {
	print "did not delete!\n";
	exit;
    }
    open (IN, "$output_config");

    while (<IN>) {
	if (/^(CONFIG\S+)=(.*)/) {
	    ${$config_ref}{$1} = $2;
	}
    }
    close(IN);
}

sub get_dependencies {
    my ($config) = @_;

    my $arr = $dependency{$config};
    if (!defined($arr)) {
	return ();
    }

    my @deps = @{$arr};

    foreach my $dep (@{$arr}) {
	print "ADD DEP $dep\n";
	@deps = (@deps, get_dependencies $dep);
    }

    return @deps;
}

sub create_config {
    my @configs = @_;

    open(OUT, ">$output_config") or dodie "Can not write to $output_config";

    foreach my $config (@configs) {
	print OUT "$config_set{$config}\n";
	my @deps = get_dependencies $config;
	foreach my $dep (@deps) {
	    print OUT "$config_set{$dep}\n";
	}
    }

    foreach my $config (keys %config_ignore) {
	print OUT "$config_ignore{$config}\n";
    }
    close(OUT);

#    exit;
    make_oldconfig;
}

sub compare_configs {
    my (%a, %b) = @_;

    foreach my $item (keys %a) {
	if (!defined($b{$item})) {
	    print "diff $item\n";
	    return 1;
	}
	delete $b{$item};
    }

    my @keys = keys %b;
    if ($#keys) {
	print "diff2 $keys[0]\n";
    }
    return -1 if ($#keys >= 0);

    return 0;
}

sub run_config_bisect_test {
    my ($type) = @_;

    return run_bisect_test $type, "oldconfig";
}

sub process_passed {
    my (%configs) = @_;

    doprint "These configs had no failure: (Enabling them for further compiles)\n";
    # Passed! All these configs are part of a good compile.
    # Add them to the min options.
    foreach my $config (keys %configs) {
	if (defined($config_list{$config})) {
	    doprint " removing $config\n";
	    $config_ignore{$config} = $config_list{$config};
	    delete $config_list{$config};
	}
    }
    doprint "config copied to $outputdir/config_good\n";
    run_command "cp -f $output_config $outputdir/config_good";
}

sub process_failed {
    my ($config) = @_;

    doprint "\n\n***************************************\n";
    doprint "Found bad config: $config\n";
    doprint "***************************************\n\n";
}

sub run_config_bisect {

    my @start_list = keys %config_list;

    if ($#start_list < 0) {
	doprint "No more configs to test!!!\n";
	return -1;
    }

    doprint "***** RUN TEST ***\n";
    my $type = $opt{"CONFIG_BISECT_TYPE[$iteration]"};
    my $ret;
    my %current_config;

    my $count = $#start_list + 1;
    doprint "  $count configs to test\n";

    my $half = int($#start_list / 2);

    do {
	my @tophalf = @start_list[0 .. $half];

	create_config @tophalf;
	read_current_config \%current_config;

	$count = $#tophalf + 1;
	doprint "Testing $count configs\n";
	my $found = 0;
	# make sure we test something
	foreach my $config (@tophalf) {
	    if (defined($current_config{$config})) {
		logit " $config\n";
		$found = 1;
	    }
	}
	if (!$found) {
	    # try the other half
	    doprint "Top half produced no set configs, trying bottom half\n";
	    @tophalf = @start_list[$half + 1 .. $#start_list];
	    create_config @tophalf;
	    read_current_config \%current_config;
	    foreach my $config (@tophalf) {
		if (defined($current_config{$config})) {
		    logit " $config\n";
		    $found = 1;
		}
	    }
	    if (!$found) {
		doprint "Failed: Can't make new config with current configs\n";
		foreach my $config (@start_list) {
		    doprint "  CONFIG: $config\n";
		}
		return -1;
	    }
	    $count = $#tophalf + 1;
	    doprint "Testing $count configs\n";
	}

	$ret = run_config_bisect_test $type;
	if ($bisect_manual) {
	    $ret = answer_bisect;
	}
	if ($ret) {
	    process_passed %current_config;
	    return 0;
	}

	doprint "This config had a failure.\n";
	doprint "Removing these configs that were not set in this config:\n";
	doprint "config copied to $outputdir/config_bad\n";
	run_command "cp -f $output_config $outputdir/config_bad";

	# A config exists in this group that was bad.
	foreach my $config (keys %config_list) {
	    if (!defined($current_config{$config})) {
		doprint " removing $config\n";
		delete $config_list{$config};
	    }
	}

	@start_list = @tophalf;

	if ($#start_list == 0) {
	    process_failed $start_list[0];
	    return 1;
	}

	# remove half the configs we are looking at and see if
	# they are good.
	$half = int($#start_list / 2);
    } while ($#start_list > 0);

    # we found a single config, try it again unless we are running manually

    if ($bisect_manual) {
	process_failed $start_list[0];
	return 1;
    }

    my @tophalf = @start_list[0 .. 0];

    $ret = run_config_bisect_test $type;
    if ($ret) {
	process_passed %current_config;
	return 0;
    }

    process_failed $start_list[0];
    return 1;
}

sub config_bisect {
    my ($i) = @_;

    my $start_config = $opt{"CONFIG_BISECT[$i]"};

    my $tmpconfig = "$tmpdir/use_config";

    if (defined($config_bisect_good)) {
	process_config_ignore $config_bisect_good;
    }

    # Make the file with the bad config and the min config
    if (defined($minconfig)) {
	# read the min config for things to ignore
	run_command "cp $minconfig $tmpconfig" or
	    dodie "failed to copy $minconfig to $tmpconfig";
    } else {
	unlink $tmpconfig;
    }

    if (-f $tmpconfig) {
	load_force_config($tmpconfig);
	process_config_ignore $tmpconfig;
    }

    # now process the start config
    run_command "cp $start_config $output_config" or
	dodie "failed to copy $start_config to $output_config";

    # read directly what we want to check
    my %config_check;
    open (IN, $output_config)
	or dodie "faied to open $output_config";

    while (<IN>) {
	if (/^((CONFIG\S*)=.*)/) {
	    $config_check{$2} = $1;
	}
    }
    close(IN);

    # Now run oldconfig with the minconfig
    make_oldconfig;

    # check to see what we lost (or gained)
    open (IN, $output_config)
	or dodie "Failed to read $start_config";

    my %removed_configs;
    my %added_configs;

    while (<IN>) {
	if (/^((CONFIG\S*)=.*)/) {
	    # save off all options
	    $config_set{$2} = $1;
	    if (defined($config_check{$2})) {
		if (defined($config_ignore{$2})) {
		    $removed_configs{$2} = $1;
		} else {
		    $config_list{$2} = $1;
		}
	    } elsif (!defined($config_ignore{$2})) {
		$added_configs{$2} = $1;
		$config_list{$2} = $1;
	    }
	}
    }
    close(IN);

    my @confs = keys %removed_configs;
    if ($#confs >= 0) {
	doprint "Configs overridden by default configs and removed from check:\n";
	foreach my $config (@confs) {
	    doprint " $config\n";
	}
    }
    @confs = keys %added_configs;
    if ($#confs >= 0) {
	doprint "Configs appearing in make oldconfig and added:\n";
	foreach my $config (@confs) {
	    doprint " $config\n";
	}
    }

    my %config_test;
    my $once = 0;

    # Sometimes kconfig does weird things. We must make sure
    # that the config we autocreate has everything we need
    # to test, otherwise we may miss testing configs, or
    # may not be able to create a new config.
    # Here we create a config with everything set.
    create_config (keys %config_list);
    read_current_config \%config_test;
    foreach my $config (keys %config_list) {
	if (!defined($config_test{$config})) {
	    if (!$once) {
		$once = 1;
		doprint "Configs not produced by kconfig (will not be checked):\n";
	    }
	    doprint "  $config\n";
	    delete $config_list{$config};
	}
    }
    my $ret;
    do {
	$ret = run_config_bisect;
    } while (!$ret);

    return $ret if ($ret < 0);

    success $i;
}

sub patchcheck_reboot {
    doprint "Reboot and sleep $patchcheck_sleep_time seconds\n";
    reboot $patchcheck_sleep_time;
}

sub patchcheck {
    my ($i) = @_;

    die "PATCHCHECK_START[$i] not defined\n"
	if (!defined($opt{"PATCHCHECK_START[$i]"}));
    die "PATCHCHECK_TYPE[$i] not defined\n"
	if (!defined($opt{"PATCHCHECK_TYPE[$i]"}));

    my $start = $opt{"PATCHCHECK_START[$i]"};

    my $end = "HEAD";
    if (defined($opt{"PATCHCHECK_END[$i]"})) {
	$end = $opt{"PATCHCHECK_END[$i]"};
    }

    # Get the true sha1's since we can use things like HEAD~3
    $start = get_sha1($start);
    $end = get_sha1($end);

    my $type = $opt{"PATCHCHECK_TYPE[$i]"};

    # Can't have a test without having a test to run
    if ($type eq "test" && !defined($run_test)) {
	$type = "boot";
    }

    open (IN, "git log --pretty=oneline $end|") or
	dodie "could not get git list";

    my @list;

    while (<IN>) {
	chomp;
	$list[$#list+1] = $_;
	last if (/^$start/);
    }
    close(IN);

    if ($list[$#list] !~ /^$start/) {
	fail "SHA1 $start not found";
    }

    # go backwards in the list
    @list = reverse @list;

    my $save_clean = $noclean;
    my %ignored_warnings;

    if (defined($ignore_warnings)) {
	foreach my $sha1 (split /\s+/, $ignore_warnings) {
	    $ignored_warnings{$sha1} = 1;
	}
    }

    $in_patchcheck = 1;
    foreach my $item (@list) {
	my $sha1 = $item;
	$sha1 =~ s/^([[:xdigit:]]+).*/$1/;

	doprint "\nProcessing commit $item\n\n";

	run_command "git checkout $sha1" or
	    die "Failed to checkout $sha1";

	# only clean on the first and last patch
	if ($item eq $list[0] ||
	    $item eq $list[$#list]) {
	    $noclean = $save_clean;
	} else {
	    $noclean = 1;
	}

	if (defined($minconfig)) {
	    build "useconfig:$minconfig" or return 0;
	} else {
	    # ?? no config to use?
	    build "oldconfig" or return 0;
	}


	if (!defined($ignored_warnings{$sha1})) {
	    check_buildlog $sha1 or return 0;
	}

	next if ($type eq "build");

	my $failed = 0;

	start_monitor_and_boot or $failed = 1;

	if (!$failed && $type ne "boot"){
	    do_run_test or $failed = 1;
	}
	end_monitor;
	return 0 if ($failed);

	patchcheck_reboot;

    }
    $in_patchcheck = 0;
    success $i;

    return 1;
}

my %depends;
my $iflevel = 0;
my @ifdeps;

# prevent recursion
my %read_kconfigs;

# taken from streamline_config.pl
sub read_kconfig {
    my ($kconfig) = @_;

    my $state = "NONE";
    my $config;
    my @kconfigs;

    my $cont = 0;
    my $line;


    if (! -f $kconfig) {
	doprint "file $kconfig does not exist, skipping\n";
	return;
    }

    open(KIN, "$kconfig")
	or die "Can't open $kconfig";
    while (<KIN>) {
	chomp;

	# Make sure that lines ending with \ continue
	if ($cont) {
	    $_ = $line . " " . $_;
	}

	if (s/\\$//) {
	    $cont = 1;
	    $line = $_;
	    next;
	}

	$cont = 0;

	# collect any Kconfig sources
	if (/^source\s*"(.*)"/) {
	    $kconfigs[$#kconfigs+1] = $1;
	}

	# configs found
	if (/^\s*(menu)?config\s+(\S+)\s*$/) {
	    $state = "NEW";
	    $config = $2;

	    for (my $i = 0; $i < $iflevel; $i++) {
		if ($i) {
		    $depends{$config} .= " " . $ifdeps[$i];
		} else {
		    $depends{$config} = $ifdeps[$i];
		}
		$state = "DEP";
	    }

	# collect the depends for the config
	} elsif ($state eq "NEW" && /^\s*depends\s+on\s+(.*)$/) {

	    if (defined($depends{$1})) {
		$depends{$config} .= " " . $1;
	    } else {
		$depends{$config} = $1;
	    }

	# Get the configs that select this config
	} elsif ($state ne "NONE" && /^\s*select\s+(\S+)/) {
	    if (defined($depends{$1})) {
		$depends{$1} .= " " . $config;
	    } else {
		$depends{$1} = $config;
	    }

	# Check for if statements
	} elsif (/^if\s+(.*\S)\s*$/) {
	    my $deps = $1;
	    # remove beginning and ending non text
	    $deps =~ s/^[^a-zA-Z0-9_]*//;
	    $deps =~ s/[^a-zA-Z0-9_]*$//;

	    my @deps = split /[^a-zA-Z0-9_]+/, $deps;

	    $ifdeps[$iflevel++] = join ':', @deps;

	} elsif (/^endif/) {

	    $iflevel-- if ($iflevel);

	# stop on "help"
	} elsif (/^\s*help\s*$/) {
	    $state = "NONE";
	}
    }
    close(KIN);

    # read in any configs that were found.
    foreach $kconfig (@kconfigs) {
	if (!defined($read_kconfigs{$kconfig})) {
	    $read_kconfigs{$kconfig} = 1;
	    read_kconfig("$builddir/$kconfig");
	}
    }
}

sub read_depends {
    # find out which arch this is by the kconfig file
    open (IN, $output_config)
	or dodie "Failed to read $output_config";
    my $arch;
    while (<IN>) {
	if (m,Linux/(\S+)\s+\S+\s+Kernel Configuration,) {
	    $arch = $1;
	    last;
	}
    }
    close IN;

    if (!defined($arch)) {
	doprint "Could not find arch from config file\n";
	doprint "no dependencies used\n";
	return;
    }

    # arch is really the subarch, we need to know
    # what directory to look at.
    if ($arch eq "i386" || $arch eq "x86_64") {
	$arch = "x86";
    } elsif ($arch =~ /^tile/) {
	$arch = "tile";
    }

    my $kconfig = "$builddir/arch/$arch/Kconfig";

    if (! -f $kconfig && $arch =~ /\d$/) {
	my $orig = $arch;
 	# some subarchs have numbers, truncate them
	$arch =~ s/\d*$//;
	$kconfig = "$builddir/arch/$arch/Kconfig";
	if (! -f $kconfig) {
	    doprint "No idea what arch dir $orig is for\n";
	    doprint "no dependencies used\n";
	    return;
	}
    }

    read_kconfig($kconfig);
}

sub read_config_list {
    my ($config) = @_;

    open (IN, $config)
	or dodie "Failed to read $config";

    while (<IN>) {
	if (/^((CONFIG\S*)=.*)/) {
	    if (!defined($config_ignore{$2})) {
		$config_list{$2} = $1;
	    }
	}
    }

    close(IN);
}

sub read_output_config {
    my ($config) = @_;

    assign_configs \%config_ignore, $config;
}

sub make_new_config {
    my @configs = @_;

    open (OUT, ">$output_config")
	or dodie "Failed to write $output_config";

    foreach my $config (@configs) {
	print OUT "$config\n";
    }
    close OUT;
}

sub get_depends {
    my ($dep) = @_;

    my $kconfig = $dep;
    $kconfig =~ s/CONFIG_//;

    $dep = $depends{"$kconfig"};

    # the dep string we have saves the dependencies as they
    # were found, including expressions like ! && ||. We
    # want to split this out into just an array of configs.

    my $valid = "A-Za-z_0-9";

    my @configs;

    while ($dep =~ /[$valid]/) {

	if ($dep =~ /^[^$valid]*([$valid]+)/) {
	    my $conf = "CONFIG_" . $1;

	    $configs[$#configs + 1] = $conf;

	    $dep =~ s/^[^$valid]*[$valid]+//;
	} else {
	    die "this should never happen";
	}
    }

    return @configs;
}

my %min_configs;
my %keep_configs;
my %save_configs;
my %processed_configs;
my %nochange_config;

sub test_this_config {
    my ($config) = @_;

    my $found;

    # if we already processed this config, skip it
    if (defined($processed_configs{$config})) {
	return undef;
    }
    $processed_configs{$config} = 1;

    # if this config failed during this round, skip it
    if (defined($nochange_config{$config})) {
	return undef;
    }

    my $kconfig = $config;
    $kconfig =~ s/CONFIG_//;

    # Test dependencies first
    if (defined($depends{"$kconfig"})) {
	my @parents = get_depends $config;
	foreach my $parent (@parents) {
	    # if the parent is in the min config, check it first
	    next if (!defined($min_configs{$parent}));
	    $found = test_this_config($parent);
	    if (defined($found)) {
		return $found;
	    }
	}
    }

    # Remove this config from the list of configs
    # do a make oldnoconfig and then read the resulting
    # .config to make sure it is missing the config that
    # we had before
    my %configs = %min_configs;
    delete $configs{$config};
    make_new_config ((values %configs), (values %keep_configs));
    make_oldconfig;
    undef %configs;
    assign_configs \%configs, $output_config;

    return $config if (!defined($configs{$config}));

    doprint "disabling config $config did not change .config\n";

    $nochange_config{$config} = 1;

    return undef;
}

sub make_min_config {
    my ($i) = @_;

    if (!defined($output_minconfig)) {
	fail "OUTPUT_MIN_CONFIG not defined" and return;
    }

    # If output_minconfig exists, and the start_minconfig
    # came from min_config, than ask if we should use
    # that instead.
    if (-f $output_minconfig && !$start_minconfig_defined) {
	print "$output_minconfig exists\n";
	if (read_yn " Use it as minconfig?") {
	    $start_minconfig = $output_minconfig;
	}
    }

    if (!defined($start_minconfig)) {
	fail "START_MIN_CONFIG or MIN_CONFIG not defined" and return;
    }

    my $temp_config = "$tmpdir/temp_config";

    # First things first. We build an allnoconfig to find
    # out what the defaults are that we can't touch.
    # Some are selections, but we really can't handle selections.

    my $save_minconfig = $minconfig;
    undef $minconfig;

    run_command "$make allnoconfig" or return 0;

    read_depends;

    process_config_ignore $output_config;

    undef %save_configs;
    undef %min_configs;

    if (defined($ignore_config)) {
	# make sure the file exists
	`touch $ignore_config`;
	assign_configs \%save_configs, $ignore_config;
    }

    %keep_configs = %save_configs;

    doprint "Load initial configs from $start_minconfig\n";

    # Look at the current min configs, and save off all the
    # ones that were set via the allnoconfig
    assign_configs \%min_configs, $start_minconfig;

    my @config_keys = keys %min_configs;

    # Remove anything that was set by the make allnoconfig
    # we shouldn't need them as they get set for us anyway.
    foreach my $config (@config_keys) {
	# Remove anything in the ignore_config
	if (defined($keep_configs{$config})) {
	    my $file = $ignore_config;
	    $file =~ s,.*/(.*?)$,$1,;
	    doprint "$config set by $file ... ignored\n";
	    delete $min_configs{$config};
	    next;
	}
	# But make sure the settings are the same. If a min config
	# sets a selection, we do not want to get rid of it if
	# it is not the same as what we have. Just move it into
	# the keep configs.
	if (defined($config_ignore{$config})) {
	    if ($config_ignore{$config} ne $min_configs{$config}) {
		doprint "$config is in allnoconfig as '$config_ignore{$config}'";
		doprint " but it is '$min_configs{$config}' in minconfig .. keeping\n";
		$keep_configs{$config} = $min_configs{$config};
	    } else {
		doprint "$config set by allnoconfig ... ignored\n";
	    }
	    delete $min_configs{$config};
	}
    }

    my $done = 0;
    my $take_two = 0;

    while (!$done) {

	my $config;
	my $found;

	# Now disable each config one by one and do a make oldconfig
	# till we find a config that changes our list.

	# Put configs that did not modify the config at the end.
	my @test_configs = keys %min_configs;
	my $reset = 1;
	for (my $i = 0; $i < $#test_configs; $i++) {
	    if (!defined($nochange_config{$test_configs[0]})) {
		$reset = 0;
		last;
	    }
	    # This config didn't change the .config last time.
	    # Place it at the end
	    my $config = shift @test_configs;
	    push @test_configs, $config;
	}

	# if every test config has failed to modify the .config file
	# in the past, then reset and start over.
	if ($reset) {
	    undef %nochange_config;
	}

	undef %processed_configs;

	foreach my $config (@test_configs) {

	    $found = test_this_config $config;

	    last if (defined($found));

	    # oh well, try another config
	}

	if (!defined($found)) {
	    # we could have failed due to the nochange_config hash
	    # reset and try again
	    if (!$take_two) {
		undef %nochange_config;
		$take_two = 1;
		next;
	    }
	    doprint "No more configs found that we can disable\n";
	    $done = 1;
	    last;
	}
	$take_two = 0;

	$config = $found;

	doprint "Test with $config disabled\n";

	# set in_bisect to keep build and monitor from dieing
	$in_bisect = 1;

	my $failed = 0;
	build "oldconfig";
	start_monitor_and_boot or $failed = 1;
	end_monitor;

	$in_bisect = 0;

	if ($failed) {
	    doprint "$min_configs{$config} is needed to boot the box... keeping\n";
	    # this config is needed, add it to the ignore list.
	    $keep_configs{$config} = $min_configs{$config};
	    $save_configs{$config} = $min_configs{$config};
	    delete $min_configs{$config};

	    # update new ignore configs
	    if (defined($ignore_config)) {
		open (OUT, ">$temp_config")
		    or die "Can't write to $temp_config";
		foreach my $config (keys %save_configs) {
		    print OUT "$save_configs{$config}\n";
		}
		close OUT;
		run_command "mv $temp_config $ignore_config" or
		    dodie "failed to copy update to $ignore_config";
	    }

	} else {
	    # We booted without this config, remove it from the minconfigs.
	    doprint "$config is not needed, disabling\n";

	    delete $min_configs{$config};

	    # Also disable anything that is not enabled in this config
	    my %configs;
	    assign_configs \%configs, $output_config;
	    my @config_keys = keys %min_configs;
	    foreach my $config (@config_keys) {
		if (!defined($configs{$config})) {
		    doprint "$config is not set, disabling\n";
		    delete $min_configs{$config};
		}
	    }

	    # Save off all the current mandidory configs
	    open (OUT, ">$temp_config")
		or die "Can't write to $temp_config";
	    foreach my $config (keys %keep_configs) {
		print OUT "$keep_configs{$config}\n";
	    }
	    foreach my $config (keys %min_configs) {
		print OUT "$min_configs{$config}\n";
	    }
	    close OUT;

	    run_command "mv $temp_config $output_minconfig" or
		dodie "failed to copy update to $output_minconfig";
	}

	doprint "Reboot and wait $sleep_time seconds\n";
	reboot $sleep_time;
    }

    success $i;
    return 1;
}

$#ARGV < 1 or die "ktest.pl version: $VERSION\n   usage: ktest.pl config-file\n";

if ($#ARGV == 0) {
    $ktest_config = $ARGV[0];
    if (! -f $ktest_config) {
	print "$ktest_config does not exist.\n";
	if (!read_yn "Create it?") {
	    exit 0;
	}
    }
} else {
    $ktest_config = "ktest.conf";
}

if (! -f $ktest_config) {
    open(OUT, ">$ktest_config") or die "Can not create $ktest_config";
    print OUT << "EOF"
# Generated by ktest.pl
#
# Define each test with TEST_START
# The config options below it will override the defaults
TEST_START

DEFAULTS
EOF
;
    close(OUT);
}
read_config $ktest_config;

if (defined($opt{"LOG_FILE"})) {
    $opt{"LOG_FILE"} = eval_option($opt{"LOG_FILE"}, -1);
}

# Append any configs entered in manually to the config file.
my @new_configs = keys %entered_configs;
if ($#new_configs >= 0) {
    print "\nAppending entered in configs to $ktest_config\n";
    open(OUT, ">>$ktest_config") or die "Can not append to $ktest_config";
    foreach my $config (@new_configs) {
	print OUT "$config = $entered_configs{$config}\n";
	$opt{$config} = $entered_configs{$config};
    }
}

if ($opt{"CLEAR_LOG"} && defined($opt{"LOG_FILE"})) {
    unlink $opt{"LOG_FILE"};
}

doprint "\n\nSTARTING AUTOMATED TESTS\n\n";

for (my $i = 0, my $repeat = 1; $i <= $opt{"NUM_TESTS"}; $i += $repeat) {

    if (!$i) {
	doprint "DEFAULT OPTIONS:\n";
    } else {
	doprint "\nTEST $i OPTIONS";
	if (defined($repeat_tests{$i})) {
	    $repeat = $repeat_tests{$i};
	    doprint " ITERATE $repeat";
	}
	doprint "\n";
    }

    foreach my $option (sort keys %opt) {

	if ($option =~ /\[(\d+)\]$/) {
	    next if ($i != $1);
	} else {
	    next if ($i);
	}

	doprint "$option = $opt{$option}\n";
    }
}

sub __set_test_option {
    my ($name, $i) = @_;

    my $option = "$name\[$i\]";

    if (defined($opt{$option})) {
	return $opt{$option};
    }

    foreach my $test (keys %repeat_tests) {
	if ($i >= $test &&
	    $i < $test + $repeat_tests{$test}) {
	    $option = "$name\[$test\]";
	    if (defined($opt{$option})) {
		return $opt{$option};
	    }
	}
    }

    if (defined($opt{$name})) {
	return $opt{$name};
    }

    return undef;
}

sub set_test_option {
    my ($name, $i) = @_;

    my $option = __set_test_option($name, $i);
    return $option if (!defined($option));

    return eval_option($option, $i);
}

# First we need to do is the builds
for (my $i = 1; $i <= $opt{"NUM_TESTS"}; $i++) {

    $iteration = $i;

    my $makecmd = set_test_option("MAKE_CMD", $i);

    $machine = set_test_option("MACHINE", $i);
    $ssh_user = set_test_option("SSH_USER", $i);
    $tmpdir = set_test_option("TMP_DIR", $i);
    $outputdir = set_test_option("OUTPUT_DIR", $i);
    $builddir = set_test_option("BUILD_DIR", $i);
    $test_type = set_test_option("TEST_TYPE", $i);
    $build_type = set_test_option("BUILD_TYPE", $i);
    $build_options = set_test_option("BUILD_OPTIONS", $i);
    $pre_build = set_test_option("PRE_BUILD", $i);
    $post_build = set_test_option("POST_BUILD", $i);
    $pre_build_die = set_test_option("PRE_BUILD_DIE", $i);
    $post_build_die = set_test_option("POST_BUILD_DIE", $i);
    $power_cycle = set_test_option("POWER_CYCLE", $i);
    $reboot = set_test_option("REBOOT", $i);
    $noclean = set_test_option("BUILD_NOCLEAN", $i);
    $minconfig = set_test_option("MIN_CONFIG", $i);
    $output_minconfig = set_test_option("OUTPUT_MIN_CONFIG", $i);
    $start_minconfig = set_test_option("START_MIN_CONFIG", $i);
    $ignore_config = set_test_option("IGNORE_CONFIG", $i);
    $run_test = set_test_option("TEST", $i);
    $addconfig = set_test_option("ADD_CONFIG", $i);
    $reboot_type = set_test_option("REBOOT_TYPE", $i);
    $grub_menu = set_test_option("GRUB_MENU", $i);
    $post_install = set_test_option("POST_INSTALL", $i);
    $reboot_script = set_test_option("REBOOT_SCRIPT", $i);
    $reboot_on_error = set_test_option("REBOOT_ON_ERROR", $i);
    $poweroff_on_error = set_test_option("POWEROFF_ON_ERROR", $i);
    $die_on_failure = set_test_option("DIE_ON_FAILURE", $i);
    $power_off = set_test_option("POWER_OFF", $i);
    $powercycle_after_reboot = set_test_option("POWERCYCLE_AFTER_REBOOT", $i);
    $poweroff_after_halt = set_test_option("POWEROFF_AFTER_HALT", $i);
    $sleep_time = set_test_option("SLEEP_TIME", $i);
    $bisect_sleep_time = set_test_option("BISECT_SLEEP_TIME", $i);
    $patchcheck_sleep_time = set_test_option("PATCHCHECK_SLEEP_TIME", $i);
    $ignore_warnings = set_test_option("IGNORE_WARNINGS", $i);
    $bisect_manual = set_test_option("BISECT_MANUAL", $i);
    $bisect_skip = set_test_option("BISECT_SKIP", $i);
    $config_bisect_good = set_test_option("CONFIG_BISECT_GOOD", $i);
    $store_failures = set_test_option("STORE_FAILURES", $i);
    $test_name = set_test_option("TEST_NAME", $i);
    $timeout = set_test_option("TIMEOUT", $i);
    $booted_timeout = set_test_option("BOOTED_TIMEOUT", $i);
    $console = set_test_option("CONSOLE", $i);
    $detect_triplefault = set_test_option("DETECT_TRIPLE_FAULT", $i);
    $success_line = set_test_option("SUCCESS_LINE", $i);
    $stop_after_success = set_test_option("STOP_AFTER_SUCCESS", $i);
    $stop_after_failure = set_test_option("STOP_AFTER_FAILURE", $i);
    $stop_test_after = set_test_option("STOP_TEST_AFTER", $i);
    $build_target = set_test_option("BUILD_TARGET", $i);
    $ssh_exec = set_test_option("SSH_EXEC", $i);
    $scp_to_target = set_test_option("SCP_TO_TARGET", $i);
    $target_image = set_test_option("TARGET_IMAGE", $i);
    $localversion = set_test_option("LOCALVERSION", $i);

    $start_minconfig_defined = 1;

    if (!defined($start_minconfig)) {
	$start_minconfig_defined = 0;
	$start_minconfig = $minconfig;
    }

    chdir $builddir || die "can't change directory to $builddir";

    foreach my $dir ($tmpdir, $outputdir) {
	if (!-d $dir) {
	    mkpath($dir) or
		die "can't create $dir";
	}
    }

    $ENV{"SSH_USER"} = $ssh_user;
    $ENV{"MACHINE"} = $machine;

    $target = "$ssh_user\@$machine";

    $buildlog = "$tmpdir/buildlog-$machine";
    $dmesg = "$tmpdir/dmesg-$machine";
    $make = "$makecmd O=$outputdir";
    $output_config = "$outputdir/.config";

    if ($reboot_type eq "grub") {
	dodie "GRUB_MENU not defined" if (!defined($grub_menu));
    } elsif (!defined($reboot_script)) {
	dodie "REBOOT_SCRIPT not defined"
    }

    my $run_type = $build_type;
    if ($test_type eq "patchcheck") {
	$run_type = $opt{"PATCHCHECK_TYPE[$i]"};
    } elsif ($test_type eq "bisect") {
	$run_type = $opt{"BISECT_TYPE[$i]"};
    } elsif ($test_type eq "config_bisect") {
	$run_type = $opt{"CONFIG_BISECT_TYPE[$i]"};
    }

    if ($test_type eq "make_min_config") {
	$run_type = "";
    }

    # mistake in config file?
    if (!defined($run_type)) {
	$run_type = "ERROR";
    }

    doprint "\n\n";
    doprint "RUNNING TEST $i of $opt{NUM_TESTS} with option $test_type $run_type\n\n";

    unlink $dmesg;
    unlink $buildlog;

    if (defined($addconfig)) {
	my $min = $minconfig;
	if (!defined($minconfig)) {
	    $min = "";
	}
	run_command "cat $addconfig $min > $tmpdir/add_config" or
	    dodie "Failed to create temp config";
	$minconfig = "$tmpdir/add_config";
    }

    my $checkout = $opt{"CHECKOUT[$i]"};
    if (defined($checkout)) {
	run_command "git checkout $checkout" or
	    die "failed to checkout $checkout";
    }

    if ($test_type eq "bisect") {
	bisect $i;
	next;
    } elsif ($test_type eq "config_bisect") {
	config_bisect $i;
	next;
    } elsif ($test_type eq "patchcheck") {
	patchcheck $i;
	next;
    } elsif ($test_type eq "make_min_config") {
	make_min_config $i;
	next;
    }

    if ($build_type ne "nobuild") {
	build $build_type or next;
    }

    if ($test_type eq "install") {
	get_version;
	install;
	success $i;
	next;
    }

    if ($test_type ne "build") {
	my $failed = 0;
	start_monitor_and_boot or $failed = 1;

	if (!$failed && $test_type ne "boot" && defined($run_test)) {
	    do_run_test or $failed = 1;
	}
	end_monitor;
	next if ($failed);
    }

    success $i;
}

if ($opt{"POWEROFF_ON_SUCCESS"}) {
    halt;
} elsif ($opt{"REBOOT_ON_SUCCESS"} && !do_not_reboot) {
    reboot;
}

doprint "\n    $successes of $opt{NUM_TESTS} tests were successful\n\n";

exit 0;
