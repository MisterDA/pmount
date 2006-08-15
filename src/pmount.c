/**
 * pmount.c - policy wrapper around 'mount' to allow mounting removable devices
 *            for normal users
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2004 Canonical Ltd.
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <getopt.h>
#include <errno.h>
#include <locale.h>
#include <langinfo.h>
#include <libintl.h>
#include <sys/stat.h>

#include "fs.h"
#include "policy.h"
#include "utils.h"
#include "luks.h"
#include "config.h"

/* error codes */
const int E_ARGS = 1;
const int E_DEVICE = 2;
const int E_MNTPT = 3;
const int E_POLICY = 4;
const int E_EXECMOUNT = 5;
const int E_UNLOCK = 6;
const int E_PID = 7;
const int E_LOCKED = 8;
const int E_INTERNAL = 100;

/**
 * Print some help.
 * @param exename Name of the executable (argv[0]).
 */
void
usage( const char* exename )
{
    printf( _("Usage:\n\n%s [options] <device> [<label>]\n\n"
    "  Mount <device> to a directory below %s if policy requirements\n"
    "  are met (see pmount(1) for details). If <label> is given, the mount point\n"
    "  will be %s/<label>, otherwise it will be %s<device>.\n"
    "  If the mount point does not exist, it will be created.\n\n"),
        exename, MEDIADIR, MEDIADIR, MEDIADIR );

    printf( _("%s --lock <device> <pid>\n"
    "  Prevent further pmounts of <device> until it is unlocked again. <pid>\n"
    "  specifies the process id the lock holds for. This allows to lock a device\n"
    "  by several independent processes and avoids indefinite locks of crashed\n"
    "  processes (nonexistant pids are cleaned before attempting a mount).\n\n"),
        exename );

    printf( _("%s --unlock <device> <pid>\n"
    "  Remove the lock on <device> for process <pid> again.\n\n"),
        exename);
    puts( _("Options:\n"
    "  -r          : force <device> to be mounted read-only\n"
    "  -w          : force <device> to be mounted read-write\n"
    "  -s, --sync  : mount <device> with the 'sync' option (default: 'async')\n"
    "  --noatime   : mount <device> with the 'noatime' option (default: 'atime')\n"
    "  -e, --exec  : mount <device> with the 'exec' option (default: 'noexec')\n"
    "  -t <fs>     : mount as file system type <fs> (default: autodetected)\n"
    "  -c <charset>: use given I/O character set (default: 'utf8' if called\n"
    "                in an UTF-8 locale, otherwise mount default)\n"
    "  -u <umask>  : use specified umask instead of the default (only for\n"
    "                file sytems which actually support umask setting)\n"
    " --passphrase <file>\n"
    "                read passphrase from file instead of the terminal\n"
    "                (only for LUKS encrypted devices)\n"
    "  -d, --debug : enable debug output (very verbose)\n"
    "  -h, --help  : print help message and exit successfuly\n"
    "  --version   : print version number and exit successfully") );
}

/**
 * Check whether the user is allowed to mount the given device to the given
 * mount point. Creates the mount point if it does not exist yet. 
 * @return 0 on success, -1 on failure
 */
int
check_mount_policy( const char* device, const char* mntpt ) 
{
    int result = device_valid( device ) &&
        !device_mounted( device, 0, NULL ) &&
        ( device_whitelisted( device ) || device_removable( device ) ) &&
        !device_locked( device ) &&
        mntpt_valid( mntpt ) &&
        !mntpt_mounted( mntpt, 0 );

    if( result ) 
        debug( "policy check passed\n" ); 
    else 
        debug( "policy check failed\n" );

    /* the policy functions deliver booleans, but we want a standard Unix
       result */
    return result ? 0 : -1;
}

/**
 * Create a mount point pathname.
 * @param device device for which a moint point is created
 * @param label if NULL, the mount point will be MEDIADIR/device, otherwise
 *        MEDIADIR/label
 * @param mntpt buffer to write the mount point pathname to
 * @param mntpt_size size of mntpt in characters
 * @return 0 on success, -1 on failure
 */
int
make_mountpoint_name( const char* device, const char* label, char* mntpt,
        size_t mntpt_size )
{
    char* d;
    int media_dir_len = strlen( MEDIADIR );

    if( label ) {
        /* ignore a leading MEDIADIR */
        if( !strncmp( label, MEDIADIR, media_dir_len ) )
            label += media_dir_len;

        if( !*label ) {
            fprintf( stderr, _("Error: label must not be empty\n") );
            return -1;
        }
        if( strlen( label ) > MAX_LABEL_SIZE ) {
            fprintf( stderr, _("Error: label too long\n") );
            return -1;
        }

        if( strchr( label, '/' ) ) {
            fprintf( stderr, _("Error: '/' must not occur in label name\n") );
            return -1;
        }

        snprintf( mntpt, mntpt_size, "%s%s", MEDIADIR, label );
    } else {
        if( strlen( device ) > MAX_LABEL_SIZE ) {
            fprintf( stderr, _("Error: device name too long\n") );
            return -1;
        }

        /* chop the DEVDIR prefix */
        if( !strncmp( device, DEVDIR, sizeof( DEVDIR )-1 ) )
            device += sizeof( DEVDIR )-1;

        /* get rid of slashes */
        d = strreplace( device, '/', '_' );

        snprintf( mntpt, mntpt_size, "%s%s", MEDIADIR, d );
        free( d );
    }

    debug( "mount point to be used: %s\n", mntpt );
    return 0;
}

/**
 * Drop all privileges and exec 'mount device'. Does not return on success, if
 * it returns, MOUNTPROG could not be executed.
 */
void
do_mount_fstab( const char* device )
{
    debug( "device %s handled by fstab, calling mount\n", device );

    /* drop all privileges and transparently call mount */
    get_root();
    if( setuid( getuid() ) ) {
        perror( _("Error: could not drop all uid privileges") );
        return;
    }

    execl( MOUNTPROG, MOUNTPROG, device, NULL );
    perror( _("Error: could not execute mount") );
}

/**
 * Raise to full privileges and call mount with given file system. Exits the
 * program immediately if MOUNTPROG cannot be executed or the given file system
 * is invalid. NOTE: This function must not exit() since it is called in a
 * lock-unlock-block.
 * @param device device node to mount
 * @param mntpt desired mount point
 * @param fsname file system name (mount option -t)
 * @param async if not 0, the device will be mounted with 'async' (i. e. write
 *        caching)
 * @param noatime if not 0, the device will be mounted with 'noatime'
 * @param exec if not 0, the device will be mounted with 'exec'
 * @param force_write 1 for forced r/w, 0 for forced r/o, -1 for kernel default
 * @param iocharset charset to use for file name conversion; NULL for mount
 *        default
 * @param umask User specified umask (NULL for default)
 * @param suppress_errors: if true, stderr is redirected to /dev/null
 * @return exit status of mount, or -1 on failure.
 */
int
do_mount( const char* device, const char* mntpt, const char* fsname, int async,
        int noatime, int exec, int force_write, const char* iocharset, const
        char* umask, int suppress_errors )
{
    const struct FS* fs;
    char ugid_opt[100];
    char umask_opt[100];
    char iocharset_opt[100];
    const char* sync_opt = ",sync";
    const char* atime_opt = ",atime";
    const char* exec_opt = ",noexec";
    const char* access_opt = NULL;
    char options[1000];

    /* check and retrieve option information for requested file system */
    if( !fsname) {
        fprintf( stderr, _("Internal error: mount_attempt: given file system name is NULL\n") );
        return -1;
    }

    fs = get_fs_info( fsname );
    if( !fs ) {
        fprintf( stderr, _("Error: invalid file system name '%s'\n"), fsname );
        return -1;
    }

    /* validate user specified umask */
    if( umask && parse_unsigned( umask, E_ARGS ) > 0777 ) {
        fprintf( stderr, _("Error: invalid umask %s\n"), umask );
        return -1;
    }

    /* assemble option string */
    *ugid_opt = *umask_opt = *iocharset_opt = 0;
    if( fs->support_ugid ) {
	struct stat statbuf;
	int gid = getgid();
	int result;

	/* if pmount is installed setgid, use that group, otherwise use the
	 * user's group */
	get_root();
	result = stat( "/proc/self/exe", &statbuf );
	drop_root();
	if( result < 0 )
	    fprintf( stderr, "Can't stat myself\n" );
	else {
	    if( statbuf.st_mode & S_ISGID )
		gid = statbuf.st_gid;
	}
	snprintf( ugid_opt, sizeof( ugid_opt ), ",uid=%i,gid=%i", 
		  getuid(), gid );
    }

    if( fs->umask )
        snprintf( umask_opt, sizeof( umask_opt ), ",umask=%s", 
                umask ? umask : fs->umask );

    if( async )
        sync_opt = ",async";

    if( noatime )
        atime_opt = ",noatime";

    if( exec )
        exec_opt = ",exec";

    if( force_write == 0 )
        access_opt = ",ro";
    else if( force_write == 1 )
        access_opt = ",rw";
    else
        access_opt = "";

    if( iocharset && fs->support_iocharset ) {
        if( !is_word_str( iocharset ) ) {
            fprintf( stderr, _("Error: invalid charset name '%s'\n"), iocharset );
            return -1;
        }
        snprintf( iocharset_opt, sizeof( iocharset_opt ), ",iocharset=%s", iocharset );
    }

    snprintf( options, sizeof( options ), "%s%s%s%s%s%s%s%s", 
            fs->options, sync_opt, atime_opt, exec_opt, access_opt, ugid_opt,
            umask_opt, iocharset_opt );

    /* go for it */
    return spawnl( SPAWN_EROOT | SPAWN_RROOT | (suppress_errors ? SPAWN_NO_STDERR : 0 ),
             MOUNTPROG, MOUNTPROG, "-t", fsname, "-o", options, device, mntpt,
             NULL );
}

/**
 * Try to call do_mount() with every supported file system until a call
 * succeeds.
 * @param device device node to mount
 * @param mntpt desired mount point
 * @param async if not 0, the device will be mounted with 'async' (i. e. write
 *        caching)
 * @param noatime if not 0, the device will be mounted with 'noatime'
 * @param exec if not 0, the device will be mounted with 'exec'
 * @param force_write 1 for forced r/w, 0 for forced r/o, -1 for kernel default
 * @param iocharset charset to use for file name conversion; NULL for mount
 *        default
 * @param umask User specified umask (NULL for default)
 * @return last return value of do_mount (i. e. 0 on success, != 0 on error)
 */
int
do_mount_auto( const char* device, const char* mntpt, int async, 
        int noatime, int exec, int force_write, const char* iocharset, 
        const char* umask )
{
    const struct FS* fs;
    int nostderr = 1;
    int result = -1;

    for( fs = get_supported_fs(); fs->fsname; ++fs ) {
        /* don't suppress stderr if we try the last possible fs */
        if( (fs+1)->fsname == NULL )
            nostderr = 0;
        result = do_mount( device, mntpt, fs->fsname, async, noatime, exec,
                force_write, iocharset, umask, nostderr );
        if( result == 0 )
            break;

	/* sometimes VFAT fails when using iocharset; try again without */
	if( iocharset )
	    result = do_mount( device, mntpt, fs->fsname, async, noatime, exec,
                    force_write, NULL, umask, nostderr );
        if( result <= 0 )
            break;
    }
    return result;
}

/**
 * Lock given device.
 * param pid pid of program that holds the lock
 * @return 0 on success, -1 on error (message is printed in this case).
 */
int
do_lock( const char* device, pid_t pid )
{
    char lockdirpath[PATH_MAX];
    char lockfilepath[PATH_MAX];
    int pidlock;

    if( assert_dir( LOCKDIR, 0 ) )
        return -1;

    make_lockdir_name( device, lockdirpath, sizeof( lockdirpath ) );

    if( assert_dir( lockdirpath, 0 ) )
        return -1;

    /* only allow to create locks for existing pids, to prevent DOS attacks */
    if( !pid_exists( pid ) ) {
        fprintf( stderr, _("Error: cannot lock for pid %u, this process does not exist\n"), pid );
        return -1;
    }

    snprintf( lockfilepath, sizeof( lockfilepath ), "%s/%u", lockdirpath, (unsigned) pid );

    /* we need root for creating the pid lock file */
    get_root();
    get_groot();
    pidlock = open( lockfilepath, O_WRONLY|O_CREAT, 0644 );
    drop_groot();
    drop_root();

    if( pidlock < 0 ) {
        fprintf( stderr, _("Error: could not create pid lock file %s: %s\n"),
                lockfilepath, strerror( errno ) );
        return -1;
    }

    close( pidlock );

    return 0;
}

/**
 * Unlock given device.
 * param pid pid of program that holds the lock
 * @return 0 on success, -1 on error (message is printed in this case).
 */
int
do_unlock( const char* device, pid_t pid )
{
    char lockdirpath[PATH_MAX];
    char lockfilepath[PATH_MAX];
    int result;

    make_lockdir_name( device, lockdirpath, sizeof( lockdirpath ) );

    /* if no lock dir exists, device is not locked */
    if( !is_dir( lockdirpath ) )
        return 0;

    /* remove pid file first */
    if( pid ) {
        snprintf( lockfilepath, sizeof( lockfilepath ), "%s/%u", lockdirpath, (unsigned) pid );

        /* we need root for removing the pid lock file */
        get_root();
        result = unlink( lockfilepath );
        drop_root();

        if( result ) {
            /* ignore nonexistant lock files, but report other errors */
            if( errno != ENOENT ) {
                fprintf( stderr, _("Error: could not remove pid lock file %s: %s\n"),
                         lockfilepath, strerror( errno ) );
                return -1;
            }
        }
    }

    /* Try to rmdir the dir. If there are still files (pid-locks) in it, this
     * will fail. */
    get_root();
    result = rmdir( lockdirpath );
    drop_root();

    if( result ) {
        if( errno == ENOTEMPTY )
            return 0;
        perror( _("Error: do_unlock: could not remove lock directory") );
        return -1;
    }

    return 0;
}

/**
 * Remove stale pid locks from device's lock directory.
 */
void
clean_lock_dir( const char* device )
{
    char lockdirpath[PATH_MAX];
    char lockfilepath[PATH_MAX];
    DIR* lockdir;
    struct dirent* lockfile;

    make_lockdir_name( device, lockdirpath, sizeof( lockdirpath ) );

    debug( "Cleaning lock directory %s\n", lockdirpath );

    get_root();
    lockdir = opendir( lockdirpath );
    drop_root();

    if( !lockdir )
        return;

    while( ( lockfile = readdir( lockdir ) ) ) {
        if( !strcmp( lockfile->d_name, "." ) || !strcmp( lockfile->d_name, "..") )
            continue;
        
        debug( "  checking whether %s is alive\n", lockfile->d_name);

        if( !pid_exists( parse_unsigned( lockfile->d_name, E_INTERNAL ) ) ) {
            debug( "  %s is dead, removing lock file\n", lockfile->d_name);
            snprintf( lockfilepath, sizeof( lockfilepath ), "%s/%s",
                    lockdirpath, lockfile->d_name );
            get_root();
            unlink( lockfilepath );
            drop_root();
        }
    }

    /* remove the directory if it got empty */
    get_root();
    rmdir( lockdirpath );
    drop_root();
}

/**
 * Entry point.
 */
int
main( int argc, char** argv )
{
    char *devarg = NULL, *arg2 = NULL;
    char mntpt[MEDIA_STRING_SIZE];
    char device[PATH_MAX], mntptdev[PATH_MAX];
    char decrypted_device[PATH_MAX];
    const char* fstab_device;
    int is_real_path = 0;
    int async = 1;
    int noatime = 0;
    int exec = 0;
    int force_write = -1; /* 0: ro, 1: rw, -1: default */
    const char* use_fstype = NULL;
    const char* iocharset = NULL;
    const char* umask = NULL;
    const char* passphrase = NULL;
    int result;

    enum { MOUNT, LOCK, UNLOCK } mode = MOUNT;

    int  option;
    static struct option long_opts[] = {
        { "help", 0, NULL, 'h'},
        { "debug", 0, NULL, 'd'},
        { "lock", 0, NULL, 'l'},
        { "unlock", 0, NULL, 'L'},
        { "sync", 0, NULL, 's' },
        { "noatime", 0, NULL, 'A' },
        { "exec", 0, NULL, 'e' },
        { "type", 1, NULL, 't' },
        { "charset", 1, NULL, 'c' },
        { "umask", 1, NULL, 'u' },
        { "passphrase", 1, NULL, 'p' },
        { "read-only", 0, NULL, 'r' },
        { "read-write", 0, NULL, 'w' },
        { "version", 0, NULL, 'V' },
        { NULL, 0, NULL, 0}
    };

    /* initialize locale */
    setlocale( LC_ALL, "" );
    bindtextdomain( "pmount", NULL );
    textdomain( "pmount" );

    /* are we root? */
    if( geteuid() ) {
        fputs( _("Error: this program needs to be installed suid root\n"), stderr );
        return E_INTERNAL;
    }

    /* drop root privileges until we really need them (still available as saved uid) */
    seteuid( getuid() );

    /* parse command line options */
    do {
        switch( option = getopt_long( argc, argv, "+hdelLsArwt:c:u:V", long_opts, NULL ) ) {
            case -1:  break;          /* end of arguments */
            case ':':
            case '?': return E_ARGS;  /* unknown argument */

            case 'h': usage( argv[0] ); return 0;

            case 'd': enable_debug = 1; break;

            case 'l': mode = LOCK; break;

            case 'L': mode = UNLOCK; break;

            case 's': async = 0; break;

            case 'A': noatime = 1; break;

            case 'e': exec = 1; break;

            case 't': use_fstype = optarg; break;

            case 'c': iocharset = optarg; break;

            case 'u': umask = optarg; break;

            case 'p': passphrase = optarg; break;

            case 'r': force_write = 0; break;

            case 'w': force_write = 1; break;

            case 'V': puts(VERSION); return 0;

            default:
                fprintf( stderr, _("Internal error: getopt_long() returned unknown value\n") );
                return E_INTERNAL;
        }
    } while( option != -1 );

    /* determine device and second (label/pid) argument */
    if( optind < argc )
        devarg = argv[optind];

    if( optind+1 < argc )
        arg2 = argv[optind+1];

    /* check number of arguments */
    if( !devarg || ( mode != MOUNT && !arg2 ) || argc > optind+2 ) {
        usage( argv[0] );
        return E_ARGS;
    }

    /* if we got a mount point, convert it to a device */
    if( fstab_has_mntpt( "/etc/fstab", devarg, mntptdev, sizeof(mntptdev) ) ) {
        debug( "resolved mount point %s to device %s\n", devarg, mntptdev );
        devarg = mntptdev;
    }

    /* get real path, if possible */
    if( realpath( devarg, device ) ) {
        debug( "resolved %s to device %s\n", devarg, device );
        is_real_path = 1;
    } else {
        debug( "%s cannot be resolved to a proper device node\n", devarg );
        snprintf( device, sizeof( device ), "%s", devarg );
    }

    /* is the device already handled by fstab? We allow is_real_path == 0 here
     * to transparently mount thinks like NFS and SMB drives */
    fstab_device = fstab_has_device( "/etc/fstab", device, NULL, NULL );
    if( mode == MOUNT && fstab_device ) {
        if( arg2 )
            fprintf( stderr, _("Warning: device %s is already handled by /etc/fstab,"
                    " supplied label is ignored\n"), fstab_device );

        do_mount_fstab( fstab_device );
        return E_EXECMOUNT;
    }

    /* pmounted devices really have to be a proper local device */
    if( !is_real_path ) {
        /* try to prepend '/dev' */
        if( strncmp( device, DEVDIR, sizeof( DEVDIR )-1 ) ) { 
            char d[PATH_MAX];
            snprintf( d, sizeof( d ), "%s%s", DEVDIR, device );
            if ( !realpath( d, device ) ) {
                perror( _("Error: could not determine real path of the device") );
                return E_DEVICE;
            }
            debug( "trying to prepend '" DEVDIR "' to device argument, now %s\n", device );
        }
    }

    /* does the device start with DEVDIR? */
    if( strncmp( device, DEVDIR, sizeof( DEVDIR )-1 ) ) { 
        fprintf( stderr, _("Error: invalid device %s (must be in /dev/)\n"), device ); 
        return E_DEVICE;
    }

    switch( mode ) {
        case MOUNT:
            /* determine mount point name; note that we use devarg instead of
             * device to preserve symlink names (like '/dev/usbflash' instead
             * of '/dev/sda1') */
            if( make_mountpoint_name( devarg, arg2, mntpt, sizeof( mntpt ) ) )
                return E_MNTPT;

            /* if no charset was set explicitly, autodetect UTF-8 */
            if( !iocharset ) {
                const char* codeset;
                codeset = nl_langinfo( CODESET );

                debug( "no iocharset given, current locale encoding is %s\n", codeset );

                if( codeset && !strcmp( codeset, "UTF-8" ) ) {
                    debug( "locale encoding uses UTF-8, setting iocharset to 'utf8'\n" );
                    iocharset = "utf8";
                }
            }

            /* clean stale locks */
            clean_lock_dir( device );

            if( check_mount_policy( device, mntpt )  )
                return E_POLICY;

            /* check for encrypted device */
            enum decrypt_status decrypt = luks_decrypt( device,
                    decrypted_device, sizeof( decrypted_device ), passphrase,
                    force_write == 0 ? 1 : 0 ); 

            switch (decrypt) {
                case DECRYPT_FAILED:
                    fprintf( stderr, _("Error: could not decrypt device (wrong passphrase?)\n") );
                    exit( E_POLICY );
                case DECRYPT_EXISTS:
                    fprintf( stderr, _("Error: mapped device already exists\n") );
                    exit( E_POLICY );
                case DECRYPT_OK:
                case DECRYPT_NOTENCRYPTED:
                    break;
                default:
                    fprintf( stderr, "Internal error: unhandled decrypt_status %i\n", 
                        (int) decrypt);
                    exit( E_INTERNAL );
            }

            /* lock the mount directory */
            debug( "locking mount point directory\n" );
            if( lock_dir( mntpt ) < 0) {
                fprintf( stderr, _("Error: could not lock the mount directory. Another pmount is probably running for this mount point.\n"));
                exit( E_LOCKED );
            }
            debug( "mount point directory locked\n" );

            /* off we go */
            if( use_fstype )
                result = do_mount( decrypted_device, mntpt, use_fstype, async, noatime,
                        exec, force_write, iocharset, umask, 0 );
            else
                result = do_mount_auto( decrypted_device, mntpt, async, noatime, exec,
                        force_write, iocharset, umask ); 

            /* unlock the mount point again */
            debug( "unlocking mount point directory\n" );
            unlock_dir( mntpt );
            debug( "mount point directory unlocked\n" );

            if( result ) {
                if( decrypt == DECRYPT_OK )
                    luks_release( decrypted_device );

                /* mount failed, delete the mount point again */
                if( remove_pmount_mntpt( mntpt ) ) {
                    perror( _("Error: could not delete mount point") );
                    return -1;
                }
                return E_EXECMOUNT;
            }

            return 0;

        case LOCK:
            if( device_valid( device ) )
                if( do_lock( device, parse_unsigned( arg2, E_PID ) ) )
                    return E_INTERNAL;
            return 0;

        case UNLOCK:
            if( device_valid( device ) )
                if( do_unlock( device, parse_unsigned( arg2, E_PID ) ) )
                    return E_UNLOCK;
            return 0;
    }

    fprintf( stderr, _("Internal error: mode %i not handled.\n"), (int) mode );
    return E_INTERNAL;
}
