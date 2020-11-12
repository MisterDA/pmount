configure_file(output: 'config.h', configuration: cdata)
version = vcs_tag(input: 'version.c.in', output: 'version.c')

shared = [
  'configuration.c',
  'conffile.c',
  'luks.c',
  'policy.c',
  'realpath.c',
  'utils.c',
]
libpmount = static_library('pmount', shared)

executable('pmount', ['pmount.c', 'fs.c', 'loop.c'], version,
           link_with: libpmount,
           dependencies: [blkid, intl],
           install: true,
           install_mode: ['rwsr-xr-x', 0, false])
executable('pumount', 'pumount.c', version,
           link_with: libpmount,
           dependencies: [intl],
           install: true,
           install_mode: ['rwsr-xr-x', 0, false])
