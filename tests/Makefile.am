TESTS = test_policy

# Not built by default
check_PROGRAMS = test_policy test_sysfs test_spawn
test_policy_SOURCES = test_policy.c 
test_policy_LDADD = ../src/libpmount-util.a
test_policy_CPPFLAGS = -I../src

# This is not a real test
test_sysfs_SOURCES = test_sysfs.c 
test_sysfs_LDADD = ../src/libpmount-util.a
test_sysfs_CPPFLAGS = -I../src

test_spawn_SOURCES = test_spawn.c 
test_spawn_LDADD = ../src/libpmount-util.a
test_spawn_CPPFLAGS = -I../src

EXTRA_PROGRAMS = parse_cf
parse_cf_SOURCES = parse_cf.c
parse_cf_LDADD = ../src/libpmount-util.a
parse_cf_CPPFLAGS = -I../src

