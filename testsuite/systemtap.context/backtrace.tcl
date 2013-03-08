set m1 0
set m2 0
set m3 0
set m4 0
set m5 0
set m6 0
set module1 0
set kernel 0
set script_exit 0
set eof_found 0

spawn stap -d kernel -d systemtap_test_module1 $srcdir/$subdir/backtrace.stp
#exp_internal 1
expect {
    -timeout 120
    "<no kernel backtrace at begin>\r\n" {
	pass "backtrace of begin probe"
	exec echo 0 > /proc/stap_test_cmd
	exp_continue
    }

    #backtrace from yyy_func2
    -re {^backtrace from module\(\"systemtap_test_module2\"\)\.function\(\"yyy_func2@[^\r\n]+\r\n} {
	incr m1
	expect {
	    -timeout 5
	    -re {^ 0x[a-f0-9]+ : yyy_func2[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m1 == 1} {incr m1}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func1[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m1 == 2} {incr m1}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : stm_write_cmd[^\[]+\[systemtap_test_module1\]\r\n} {
		if {$m1 == 3} {incr module1}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : [a-z_]+[^\[]+\[kernel\]\r\n} {
		if {$module1 == 1} {incr kernel}
		# we expect at least one [kernel] frame, maybe more.
	    }
	    eof { set eof_found 1; fail "early test shutdown (m1 eof)" }
	}
	if {! $eof_found} { exp_continue }
    }
    -re {.*--- yyy_func2 ---\r\nthe stack is 0x[a-f0-9]+ [^\r\n]+\r\n} {
	incr m2
	expect {
	    -timeout 5
	    -re {^ 0x[a-f0-9]+ : yyy_func2[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m2 == 1} {incr m2}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func1[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m2 == 2} {incr m2}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : stm_write_cmd[^\[]+\[systemtap_test_module1\]\r\n} {
		if {$m2 == 3} {incr module1}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : [a-z_]+[^\[]+\[kernel\]\r\n} {
		if {$module1 == 2} {incr kernel}
		# we expect at least one [kernel] frame, maybe more.
	    }
	    eof { set eof_found 1; fail "early test shutdown (m2 eof)" }
	}
	if {! $eof_found} { exp_continue }
    }

    #backtrace from yyy_func3
    -re {.*backtrace from module\(\"systemtap_test_module2\"\)\.function\(\"yyy_func3@[^\r\n]+\r\n} {
	incr m3
	expect {
	    -timeout 5
	    -re {^ 0x[a-f0-9]+ : yyy_func3[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m3 == 1} {incr m3}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func2[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m3 == 2} {incr m3}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func1[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m3 == 3} {incr m3}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : stm_write_cmd[^\[]+\[systemtap_test_module1\]\r\n} {
		if {$m3 == 4} {incr module1}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : [a-z_]+[^\[]+\[kernel\]\r\n} {
		if {$module1 == 3} {incr kernel}
		# we expect at least one [kernel] frame, maybe more.
	    }
	    eof { set eof_found 1; fail "early test shutdown (m3 eof)" }
	}
	if {! $eof_found} { exp_continue }
    }
    -re {.*--- yyy_func3 ---\r\nthe stack is 0x[a-f0-9]+ [^\r\n]+\r\n} {
	incr m4
	expect {
	    -timeout 5
	    -re {^ 0x[a-f0-9]+ : yyy_func3[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m4 == 1} {incr m4}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func2[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m4 == 2} {incr m4}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func1[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m4 == 3} {incr m4}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : stm_write_cmd[^\[]+\[systemtap_test_module1\]\r\n} {
		if {$m4 == 4} {incr module1}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : [a-z_]+[^\[]+\[kernel\]\r\n} {
		if {$module1 == 4} {incr kernel}
		# we expect at least one [kernel] frame, maybe more.
	    }
	    eof { set eof_found 1; fail "early test shutdown (m4 eof)" }
	}
	if {! $eof_found} { exp_continue }
    }

    #backtrace from yyy_func4
    -re {.*backtrace from module\(\"systemtap_test_module2\"\)\.function\(\"yyy_func4@[^\r\n]+\r\n} {
	incr m5
	expect {
	    -timeout 5
	    -re {^ 0x[a-f0-9]+ : yyy_func4[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m5 == 1} {incr m5}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func3[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m5 == 2} {incr m5}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func2[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m5 == 3} {incr m5}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func1[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m5 == 4} {incr m5}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : stm_write_cmd[^\[]+\[systemtap_test_module1\]\r\n} {
		if {$m5 == 5} {incr module1}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : [a-z_]+[^\[]+\[kernel\]\r\n} {
		if {$module1 == 5} {incr kernel}
		# we expect at least one [kernel] frame, maybe more.
	    }
	    eof { set eof_found 1; fail "early test shutdown (m5 eof)" }
	}
	if {! $eof_found} { exp_continue }
    }
    -re {.*--- yyy_func4 ---\r\nthe stack is 0x[a-f0-9]+ [^\r\n]+\r\n} {
	incr m6
	expect {
	    -timeout 5
	    -re {^ 0x[a-f0-9]+ : yyy_func4[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m6 == 1} {incr m6}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func3[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m6 == 2} {incr m6}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func2[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m6 == 3} {incr m6}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : yyy_func1[^\[]+\[systemtap_test_module2\]\r\n} {
		if {$m6 == 4} {incr m6}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : stm_write_cmd[^\[]+\[systemtap_test_module1\]\r\n} {
		if {$m6 == 5} {incr module1}
		exp_continue
	    }
	    -re {^ 0x[a-f0-9]+ : [a-z_]+[^\[]+\[kernel\]\r\n} {
		if {$module1 == 6} {incr kernel}
		# we expect at least one [kernel] frame, maybe more.
	    }
	    eof { set eof_found 1; fail "early test shutdown (m6 eof)" }
	}
	if {! $eof_found} { exp_continue }
    }
    eof {
	# good backtrace.stp called exit().
	incr script_exit;
    }
}
exec kill -INT -[exp_pid]
if {$m1 == 3} {
    pass "backtrace of yyy_func2"
} else {
    fail "backtrace of yyy_func2 ($m1)"
}
if {$m2 == 3} {
    pass "print_syms of yyy_func2"
} else {
    fail "print_syms of yyy_func2 ($m2)"
}
if {$m3 == 4} {
    pass "backtrace of yyy_func3"
} else {
    fail "backtrace of yyy_func3 ($m3)"
}
if {$m4 == 4} {
    pass "print_syms of yyy_func3"
} else {
    fail "print_syms of yyy_func3 ($m4)"
}
if {$m5 == 5} {
    pass "backtrace of yyy_func4"
} else {
    fail "backtrace of yyy_func4 ($m5)"
}
if {$m6 == 5} {
    pass "print_syms of yyy_func4"
} else {
    fail "print_syms of yyy_func4 ($m6)"
}
if {$module1 == 6} {
    pass "print_syms found systemtap_test_module1"
} else {
    fail "print_syms didn't find systemtap_test_module1 ($module1)"
}
if {$kernel >= 6} {
    pass "print_syms found \[kernel\]"
} else {
    fail "print_syms didn't find \[kernel\] ($kernel)"
}
if {$script_exit == 1} {
    pass "backtrace.stp called exit"
} else {
    fail "backtrace.stp didn't call exit ($script_exit)"
}

catch { close }
wait
