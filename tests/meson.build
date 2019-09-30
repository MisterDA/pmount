src = [ '../src/conffile.c', '../src/configuration.c', '../src/fs.c',
        '../src/loop.c', '../src/luks.c', '../src/policy.c',
        '../src/utils.c', '../src/shared.c' ]

policy = executable('policy', 'test_policy.c', src,
                    include_directories : '../src')
sysfs = executable('sysfs', 'test_sysfs.c', src,
                   include_directories : '../src')
spawn = executable('spawn', 'test_spawn.c', src,
                   include_directories : '../src')
parse_cf = executable('parse_cf', 'test_parse_cf.c', src,
                      include_directories : '../src')
test('spawn', spawn)
test('sysfs', sysfs, args : ['/dev/sda1'])
test('policy', policy)
test('parse_cf', parse_cf)
