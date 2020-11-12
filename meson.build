project('pmount',
        'c',
        version: '0.9.99-alpha',
        default_options: [
          'c_std=c18',
          'warning_level=2'
        ])

blkid = dependency('blkid', required: false)
bash_comp = dependency('bash-completion', required: false)

cdata = configuration_data()

cdata.set10('HAVE_BLKID', blkid.found())
if not blkid.found()
  message('Missing blkid library: you will not have fs autodetection (install libblkid-dev or similar?')
endif

prefix = get_option('prefix')
datadir = prefix / get_option('datadir')
sysconfdir = prefix / get_option('sysconfdir')
sharedstatedir = prefix / get_option('sharedstatedir')

if get_option('bash-completions')
  bash_files = files('completions' / 'bash' / 'pmount')
  if bash_comp.found()
    bash_install_dir = bash_comp.get_pkgconfig_variable(
      'completionsdir', define_variable: ['datadir', datadir])
  else
    bash_install_dir = datadir / 'bash-completion' / 'completions'
  endif
  install_data(bash_files, install_dir: bash_install_dir)
endif

cdata.set10('CRYPTSETUP_RUID', get_option('ruid-root-cryptsetup'))
if get_option('ruid-root-cryptsetup')
  message('Will be running cryptsetup with EUID = RUID = 0.')
endif

if not get_option('media-dir').endswith('/')
  cdata.set_quoted('MEDIADIR', get_option('media-dir') + '/')
else
  cdata.set_quoted('MEDIADIR', get_option('media-dir'))
endif
cdata.set_quoted('LOCKDIR',  sharedstatedir / 'pmount-locks')

cdata.set_quoted('ALLOWLIST', sysconfdir / 'pmount.allow')
cdata.set_quoted('SYSTEM_CONFFILE', sysconfdir / 'pmount.conf')

cdata.set_quoted('MOUNTPROG', get_option('mount-prog'))
cdata.set_quoted('UMOUNTPROG', get_option('umount-prog'))
cdata.set_quoted('CRYPTSETUPPROG', get_option('cryptsetup-prog'))
cdata.set_quoted('MOUNT_NTFS_3G', get_option('mount-ntfs-3g'))
cdata.set_quoted('FSCKPROG', get_option('fsck-prog'))
cdata.set_quoted('LOSETUPPROG', get_option('losetup-prog'))

install_data(
  files('etc' / 'pmount.conf', 'etc' / 'pmount.allow'),
  install_dir: sysconfdir)

install_man('man' / 'pmount.1', 'man' / 'pmount.conf.5', 'man' / 'pumount.1')

subdir('po')
subdir('src')
# subdir('tests')
