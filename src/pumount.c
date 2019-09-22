/**
 * pumount.c - policy wrapper around 'umount' to allow mounting removable
 *             devices for normal users
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2004 Canonical Ltd.
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <limits.h>
#include <getopt.h>
#include <libintl.h>
#include <locale.h>

#include "policy.h"
#include "utils.h"
#include "luks.h"
#include "config.h"
#include "configuration.h"

/* error codes */
const int E_ARGS = 1;
const int E_DEVICE = 2;
const int E_POLICY = 4;
const int E_EXECUMOUNT = 5;
const int E_DISALLOWED = 9;
const int E_INTERNAL = 100;

static char mntpt[MEDIA_STRING_SIZE];

/**
 * Print some help.
 * @param exename Name of the executable (argv[0]).
 */
static void
usage( const char* exename )
{
    printf( _("Usage:\n\n%s [options] <device>\n"
    "  Umount <device> from a directory below %s if policy requirements\n"
    "  are met (see pumount(1) for details). The mount point directory is removed\n"
    "  afterwards.\n\n"
    "Options:\n"
    "  -l, --lazy   : umount lazily, see umount(8)\n"
    "  --luks-force : luksClose devices pmount didn't open\n"
    "  -d, --debug  : enable debug output (very verbose)\n"
    "  -h, --help   : print help message and exit successfuly\n"
    "  --version    : print version number and exit successfully\n"),
        exename, MEDIADIR );
}

/**
 * Check whether the user is allowed to umount the given device.
 * @param ok_if_inexistant whether it is allowed for the device to not
 *        exist, as should be the case when the device has gone
 *        missing for some reason
 * @return 0 on success, -1 on failure
 */
static int
check_umount_policy( const char* device, int ok_if_inexistant )
{
    int devvalid;
    char mediadir[PATH_MAX];

    devvalid = ( ok_if_inexistant || device_valid( device ) ) &&
        device_mounted( device, 1, mntpt );

    if( !devvalid )
        return -1;

    /* paranoid check */
    if( !*mntpt ) {
        fputs( _("Internal error: could not determine mount point\n"), stderr );
        exit( E_INTERNAL );
    }

    /* MEDIADIR may be a symlink (for read-only root systems) */
    if( NULL == realpath( MEDIADIR, mediadir ) ) {
        fprintf( stderr, _("Error: could not find real path of %s\n"),
                MEDIADIR );
        exit( E_INTERNAL );
    }

    /* mount point must be below MEDIADIR */
    if( strncmp( mntpt, mediadir, strlen( mediadir ) ) ) {
        fprintf( stderr, _("Error: mount point %s is not below %s\n"), mntpt,
                MEDIADIR );
        return -1;
    }

    debug( "policy check passed\n" );
    return 0;
}

/**
 * Drop all privileges and exec 'umount device'. Does not return on success, if
 * it returns, UMOUNTPROG could not be executed.
 * @param lazy 0 for normal umount, 1 for lazy umount
 */
static void
do_umount_fstab( const char* device, int lazy, const char * fstab_mntpt )
{
    /* drop all privileges */
    get_root();
    if( setuid( getuid() ) ) {
        perror( _("Error: could not drop all uid privileges") );
        return;
    }

    debug( "device %s handled by fstab, calling umount\n", device );
    if(! strncmp(device, "LABEL=", 6) || ! strncmp(device, "UUID=", 5)) {
      debug( "'%s' is a label/uuid specification, using mount point %s to umount\n",
	     device, fstab_mntpt);
      device = fstab_mntpt;	/* device is now the mount point */
    }

    if( lazy )
        execl( UMOUNTPROG, UMOUNTPROG, "-l", device, NULL );
    else
        execl( UMOUNTPROG, UMOUNTPROG, device, NULL );
    perror( _("Error: could not execute umount") );
}

/**
 * Raise to full privileges and mounts device to mntpt.
 * @param device full device name
 * @param do_lazy 0 for normal umount, 1 for lazy umount
 * @return 0 on success, -1 if UMOUNTPROG could not be executed.
 */
static int
do_umount( const char* device, int do_lazy )
{
    int status;

    if( do_lazy )
        status = spawnl( SPAWN_EROOT|SPAWN_RROOT, UMOUNTPROG, UMOUNTPROG,
			 "-d", "-l",
                device, NULL );
    else
        status = spawnl( SPAWN_EROOT|SPAWN_RROOT, UMOUNTPROG, UMOUNTPROG,
			 "-d",
                device, NULL );

    if( status != 0 ) {
        fputs( _("Error: umount failed\n"), stderr );
        return -1;
    }

    return 0;
}

/**
 * Entry point.
 *
 */
int
main( int argc, char** argv )
{
    char device[PATH_MAX], mntptdev[PATH_MAX], path[PATH_MAX];
    const char* fstab_device, *mntptdevpath;
    char fstab_mntpt[MEDIA_STRING_SIZE];
    int is_real_path = 0;
    int do_lazy = 0;

    int  option;
    static struct option long_opts[] = {
        { "help", 0, NULL, 'h'},
        { "debug", 0, NULL, 'd'},
        { "lazy", 0, NULL, 'l'},
        { "yes-I-really-want-lazy-unmount", 0, NULL, 'R'},
        { "luks-force", 0, NULL, 'L'},
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

    if( conffile_system_read() ) {
	fputs( _("Error while reading system configuration file\n"), stderr );
	return E_INTERNAL;
    }


    /* drop root privileges until we really need them (still available as saved uid) */
    seteuid( getuid() );

    /* parse command line options */
    do {
        switch( option = getopt_long( argc, argv, "+hdluV", long_opts, NULL ) ) {
            case -1:        break;          /* end of arguments */
            case '?':        return E_ARGS;  /* unknown argument */

            case 'h':        usage( argv[0] ); return 0;

            case 'd':   enable_debug = 1; break;

            case 'l':
	      fputs(_("WARNING: Lazy unmount are likely to jeopardize data "
		      "integrity on removable devices.\n"
		      "If that's what you really want, run pumount with "
		      "--yes-I-really-want-lazy-unmount\nAborting.\n"),
		    stderr);
	      return 1;
	    case 'R':
	      do_lazy = 1; break;

            case 'L': /* was not used, keep the option as NOP */; break;

            case 'V': puts(PMOUNT_VERSION); return 0;

            default:
                fputs( _("Internal error: getopt_long() returned unknown value\n"), stderr );
                return E_INTERNAL;
        }
    } while( option != -1 );

    /* invalid number of args? */
    if( optind + 1 != argc ) {
        usage( argv[0] );
        return E_ARGS;
    }

    /* Check if the user is physically logged in */
    ensure_user_physically_logged_in(argv[0]);

    /* if we got a mount point, convert it to a device */
    mntptdevpath = argv[optind];
    debug ("checking whether %s is a mounted directory\n", mntptdevpath);
    if( fstab_has_mntpt( "/proc/mounts", mntptdevpath, mntptdev, sizeof(mntptdev) ) ) {
        debug( "resolved mount point %s to device %s\n", mntptdevpath, mntptdev );
        mntptdevpath = mntptdev;
    } else if( !strchr( mntptdevpath, '/' ) ) {
        /* try to prepend MEDIADIR */
        snprintf( path, sizeof( path ), "%s%s", MEDIADIR, mntptdevpath );
        debug ("checking whether %s is a mounted directory\n", path);
        if( fstab_has_mntpt( "/proc/mounts", path, mntptdev, sizeof(mntptdev) ) ) {
            debug( "resolved mount point %s to device %s\n", path, mntptdev );
            mntptdevpath = mntptdev;
        }
    }

    /* get real path, if possible */
    if( realpath( mntptdevpath, device ) ) {
        debug( "resolved %s to device %s\n", mntptdevpath, device );
        is_real_path = 1;
    } else {
        debug( "%s cannot be resolved to a proper device node\n", mntptdevpath );
        snprintf( device, sizeof( device ), "%s", mntptdevpath );
    }

    /* is the device already handled by fstab? */
    fstab_device = fstab_has_device( "/etc/fstab", device, fstab_mntpt, NULL );
    if( fstab_device ) {
        do_umount_fstab( fstab_device, do_lazy, fstab_mntpt );
        return E_EXECUMOUNT;
    }

    /* we cannot really check the real path when unmounting lazily since the
     * device node might not exist any more */
    if( !is_real_path && !do_lazy ) {
        /* try to prepend '/dev' */
        if( strncmp( device, DEVDIR, sizeof( DEVDIR )-1 ) ) {
            char *d;
            if ( asprintf( &d, "%s%s", DEVDIR, device ) < 0) {
                perror("asprintf");
                return E_INTERNAL;
            }
            if ( !realpath( d, device ) ) {
                perror( _("Error: could not determine real path of the device") );
                free(d);
                return E_DEVICE;
            }
            free(d);
            debug( "trying to prepend '" DEVDIR
		   "' to device argument, now '%s'\n", device );
	    /* We need to lookup again in fstab: */
	    fstab_device = fstab_has_device( "/etc/fstab", device,
					     fstab_mntpt, NULL );
	    if( fstab_device ) {
	      do_umount_fstab( fstab_device, do_lazy, fstab_mntpt );
	      return E_EXECUMOUNT;
	    }
	}
    }

    /* does the device start with DEVDIR? */
    if( strncmp( device, DEVDIR, sizeof( DEVDIR )-1 ) ) {
        fprintf( stderr, _("Error: invalid device %s (must be in /dev/)\n"), device );
        return E_DEVICE;
    }

    /* check if we have a dmcrypt device */
    if( luks_get_mapped_device( device, device, sizeof( device ) ) )
        debug( "Unmounting mapped device %s instead.\n", device );

    /* Now, we accept when devices have gone missing */
    if( check_umount_policy( device, 1 ) )
        return E_POLICY;

    /* go for it */
    if( do_umount( device, do_lazy ) )
        return E_EXECUMOUNT;

    /* release LUKS device, if appropriate */
    luks_release( device, 1 );

    /* delete mount point */
    remove_pmount_mntpt( mntpt );

    return 0;
}
