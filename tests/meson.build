policy = executable('policy', 'test_policy.c',
                    link_with: libpmount,
                    include_directories: '../src')
sysfs = executable('sysfs', 'test_sysfs.c',
                   link_with: libpmount,
                   include_directories: '../src')
spawn = executable('spawn', 'test_spawn.c',
                   link_with: libpmount,
                   include_directories: '../src')
parse_cf = executable('parse_cf', 'test_parse_cf.c',
                      link_with: libpmount,
                      include_directories: '../src')

testdir = meson.source_root() / meson.current_source_dir()

test('spawn', spawn)
test('parse_cf', parse_cf, args: [testdir / 'parse_cf.conf'])
test('policy', find_program(testdir / 'test_policy.sh'),
     args: [policy])

# Change /dev/sda1 to a suitable block device
# test('sysfs', sysfs, args: ['/dev/sda1'])
