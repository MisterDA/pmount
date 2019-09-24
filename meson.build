project('pmount', 'c', version : '0.10-alpha')

prefix     = get_option('prefix')
datadir    = join_paths(prefix, get_option('datadir'))
sysconfdir = join_paths(prefix, get_option('sysconfdir'))

install_data(files('etc/pmount.conf', 'etc/pmount.allow'),
             install_dir : sysconfdir)

if get_option('bash-completions')
  bash_files = files('completions/bash/pmount')
  bash_install_dir = datadir + '/bash-completion/completions'
  install_data(bash_files, install_dir: bash_install_dir)
endif

version = '"@0@"'.format(meson.project_version())
git = find_program('git', native: true, required: false)
if git.found()
  git_commit = run_command([git, 'rev-parse', '--short', 'HEAD'])
  git_branch = run_command([git, 'rev-parse', '--abbrev-ref', 'HEAD'])
  if git_commit.returncode() == 0 and git_branch.returncode() == 0
    version = '"@0@-@1@ (" __DATE__ ", branch \'@2@\')"'.format(
      meson.project_version(),
      git_commit.stdout().strip(),
      git_branch.stdout().strip(),
    )
  endif
endif
add_project_arguments('-DPMOUNT_VERSION=@0@'.format(version), language: 'c')

blkid = dependency('blkid', required : false)
if not blkid.found()
  message('Missing blkid library: you will not have fs autodetection (install libblkid-dev or similar?')
endif

if get_option('ruid-root-cryptsetup')
  message('Will be running cryptsetup with EUID = RUID = 0.')
endif

cdata = configuration_data({
  'CRYPTSETUP_RUID' : get_option('ruid-root-cryptsetup'),
  'CRYPTSETUP'      : '"' + get_option('cryptsetup-prog') + '"',
  'FSCKPROG'        : '"' + get_option('fsck-prog') + '"',
  'LOSETUPPROG'     : '"' + get_option('losetup-prog') + '"',
  'MOUNTPROG'       : '"' + get_option('mount-prog') + '"',
  'MOUNT_NTFS_3G'   : '"' + get_option('mount-ntfs-3g') + '"',
  'UMOUNTPROG'      : '"' + get_option('umount-prog') + '"',
  'LOCKDIR'         : '"' + get_option('lock-dir') + '"',
  'MEDIADIR'        : '"' + get_option('media-dir') + '"',
  'SYSTEM_CONFFILE' : '"' + sysconfdir + '/pmount.conf"',
  'WHITELIST'       : '"' + sysconfdir + '/pmount.allow"',
  'HAVE_BLKID'      : blkid.found(),
})

subdir('po')
subdir('man')
subdir('src')
