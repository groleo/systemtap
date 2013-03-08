# Only run on make installcheck
if {! [installtest_p]} { untested "ustack-$testname"; return }
if {! [uprobes_p]} { untested "ustack-$testname"; return }

# Output is:
#print_ubacktrace exe 0
# 0x080484ba : main_func+0xa/0x29 [.../uprobes_exe]
# 0x080484f6 : main+0x1d/0x37 [.../uprobes_exe]
#print_usyms exe 1
# 0x080484ba : main_func+0xa/0x29 [.../uprobes_exe]
# 0x080484c9 : main_func+0x19/0x29 [.../uprobes_exe]
# 0x080484f6 : main+0x1d/0x37 [.../uprobes_exe]
#print_ubacktrace lib 2
# 0x00db2422 : lib_func+0x16/0x2b [.../libuprobes_lib.so]
# 0x00db2455 : lib_main+0x1e/0x29 [.../libuprobes_lib.so]
# 0x080484d0 : main_func+0x20/0x29 [.../uprobes_exe]
# 0x080484c9 : main_func+0x19/0x29 [.../uprobes_exe]
# 0x080484c9 : main_func+0x19/0x29 [.../uprobes_exe]
# 0x080484f6 : main+0x1d/0x37 [.../uprobes_exe]
#print_usyms lib 3
# 0x00db2422 : lib_func+0x16/0x2b [.../libuprobes_lib.so]
# 0x00db2431 : lib_func+0x25/0x2b [.../libuprobes_lib.so]
# 0x00db2455 : lib_main+0x1e/0x29 [.../libuprobes_lib.so]
# 0x080484d0 : main_func+0x20/0x29 [.../uprobes_exe]
# 0x080484c9 : main_func+0x19/0x29 [.../uprobes_exe]
# 0x080484c9 : main_func+0x19/0x29 [.../uprobes_exe]
# 0x080484f6 : main+0x1d/0x37 [.../uprobes_exe]

set print 0
set main 0
set main_func 0
set lib_main 0
set lib_func 0
set start_func 0
send_log "Running: stap -w --ldd $srcdir/$subdir/ustack.stp $testexe $testlib -c $testexe\n"
spawn stap -w --ldd $srcdir/$subdir/ustack.stp $testexe $testlib -c $testexe

expect {
  -timeout 60
    -re {^print_[^\r\n]+\r\n} {incr print; exp_continue}
    -re {^ 0x[a-f0-9]+ : main\+0x[^\r\n]+\r\n} {incr main; exp_continue}
    -re {^ 0x[a-f0-9]+ : main_func\+0x[^\r\n]+\r\n} {incr main_func; exp_continue}
    -re {^ 0x[a-f0-9]+ : lib_main\+0x[^\r\n]+\r\n} {incr lib_main; exp_continue}
    -re {^ 0x[a-f0-9]+ : lib_func\+0x[^\r\n]+\r\n} {incr lib_func; exp_continue}
    -re {^ 0x[a-f0-9]+ : _[^\r\n]+\r\n} {incr start_func; exp_continue}
    -re {^ 0x[a-f0-9]+ : [^\r\n]+/libc-[^\r\n]+\r\n} {incr start_func; exp_continue}
    timeout { fail "ustack-$testname (timeout)"; exec kill -INT -[exp_pid] }
    eof { }
}
# kill again for good measure
exec kill -INT -[exp_pid]
catch {close}; catch {wait}

if {$print == 4} { pass "ustack-$testname print" } {
    fail "ustack-$testname print ($print)"
}

if {$main == 4} { pass "ustack-$testname main" } {
    fail "ustack-$testname main ($main)"
}

if {$main_func == 9} { pass "ustack-$testname main_func" } {
    fail "ustack-$testname main_func ($main_func)"
}

if {$lib_main == 2} { pass "ustack-$testname lib_main" } {
    fail "ustack-$testname lib_main ($lib_main)"
}

if {$lib_func == 3} { pass "ustack-$testname lib_func" } {
    fail "ustack-$testname lib_func ($lib_func)"
}
