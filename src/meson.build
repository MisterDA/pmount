src = [ 'conffile.c', 'configuration.c', 'fs.c', 'loop.c', 'luks.c',
  'policy.c', 'utils.c', ]

configure_file(output : 'config.h', configuration : cdata)

executable('pmount', src, 'pmount.c', dependencies : blkid, install : true,
           install_mode: ['rwsr-xr-x', 0, false])
executable('pumount', src, 'pumount.c', install : true,
           install_mode: ['rwsr-xr-x', 0, false])
