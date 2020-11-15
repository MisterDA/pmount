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
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <limits.h>
#include <getopt.h>
#include <libintl.h>
#include <locale.h>
#include <unistd.h>
#include <errno.h>

#include "policy.h"
#include "utils.h"
#include "luks.h"
#include "configuration.h"

extern const char* VERSION;

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
    "  -h, --help   : print help message and exit successfully\n"
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
    char *mediadir;

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
    if( ! (mediadir = realpath( MEDIADIR, NULL ) ) ) {
        fprintf( stderr, "realpath(%s): %s\n", MEDIADIR, strerror( errno ) );
        exit( E_INTERNAL );
    }

    /* mount point must be below MEDIADIR */
    if( strncmp( mntpt, mediadir, strlen( mediadir ) ) != 0 ) {
        fprintf( stderr, _("Error: mount point %s is not below %s\n"), mntpt,
                MEDIADIR );
        free( mediadir );
        return -1;
    }

    debug( "policy check passed\n" );
    free( mediadir );
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
main( int argc, char * const argv[] )
{
    char *devarg = NULL, *device;
    char *mntptdev = NULL;
    const char* fstab_device;
    char fstab_mntpt[MEDIA_STRING_SIZE];
    int is_real_path = 0;
    int do_lazy = 0;

    struct option long_opts[] = {
        { "debug", 0, NULL, 'd' },
        { "help", 0, NULL, 'h' },
        { "lazy", 0, NULL, 'l' },
        { "luks-force", 0, NULL, 'L' },
        { "version", 0, NULL, 'V' },
        { "yes-I-really-want-lazy-unmount", 0, &do_lazy, 1 },
        { NULL, 0, NULL, 0 }
    };

    /* initialize locale */
    setlocale( LC_ALL, "" );
    bindtextdomain( "pmount", NULL );
    textdomain( "pmount" );

    /* parse command line options */
    while(1) {
        int option = getopt_long( argc, argv, "+dhlV", long_opts, NULL );
        if( option == -1 ) /* end of arguments */
            break;
        switch( option ) {
        case '?': return E_ARGS;  /* unknown argument */
        case 'd': enable_debug = 1; break;
        case 'h': usage( argv[0] ); return EXIT_SUCCESS;
        case 'l':
            fputs(_("WARNING: Lazy unmount are likely to jeopardize data "
                    "integrity on removable devices.\n"
                    "If that's what you really want, run pumount with "
                    "--yes-I-really-want-lazy-unmount\nAborting.\n"),
                  stderr);
            return EXIT_FAILURE;
        case 'V': puts(VERSION); return EXIT_SUCCESS;
        default:
            fputs( _("Internal error: getopt_long() returned unknown value\n"),
                   stderr );
            return E_INTERNAL;
        }
    }

    /* invalid number of args? */
    if( optind + 1 != argc ) {
        usage( argv[0] );
        return E_ARGS;
    }

    devarg = argv[optind];

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
    if( seteuid( getuid() ) ) {
        perror( _("Error: could not drop all effective uid privileges") );
        return E_INTERNAL;
    }

    /* Check if the user is physically logged in */
    ensure_user_physically_logged_in(argv[0]);

    /* if we got a mount point, convert it to a device */
    debug ("checking whether %s is a mounted directory\n", devarg);
    if( fstab_has_mntpt( "/proc/mounts", devarg, &mntptdev ) ) {
        debug( "resolved mount point %s to device %s\n", devarg, mntptdev );
        devarg = mntptdev;
    } else if( !strchr( devarg, '/' ) ) {
        char *path;
        /* try to prepend MEDIADIR */
        if (asprintf(&path, "%s%s", MEDIADIR, devarg) == -1) {
            perror("asprintf");
            return E_INTERNAL;
        }
        debug ("checking whether %s is a mounted directory\n", path);
        if( fstab_has_mntpt( "/proc/mounts", path, &mntptdev ) ) {
            debug( "resolved mount point %s to device %s\n", path, mntptdev );
            devarg = mntptdev;
        }
        free( path );
    }

    /* get real path, if possible */
    if( ( device = realpath( devarg, NULL ) ) ) {
        debug( "resolved %s to device %s\n", devarg, device );
        is_real_path = 1;
    } else {
        debug( "realpath(%s): %s\n", devarg, strerror( errno ) );
        device = strdup( devarg );
        if(! device ) {
            perror("strdup(devarg)");
            return E_INTERNAL;
        }
    }

    /* is the device already handled by fstab? */
    fstab_device = fstab_has_device( "/etc/fstab", device, fstab_mntpt, NULL );
    if( fstab_device ) {
        do_umount_fstab( fstab_device, do_lazy, fstab_mntpt );
        free( device );
        return E_EXECUMOUNT;
    }

    /* we cannot really check the real path when unmounting lazily since the
     * device node might not exist any more */
    if( !is_real_path && !do_lazy ) {
        /* try to prepend '/dev' */
        if( strncmp( device, DEVDIR, sizeof( DEVDIR )-1 ) != 0) {
            char *dev_device, *realpath_dev_device;
            if( asprintf(&dev_device, "%s%s", DEVDIR, device ) ) {
                perror("asprintf");
                free( device );
                return E_INTERNAL;
            }
            if ( !(realpath_dev_device = realpath( dev_device, NULL ) ) ) {
                fprintf( stderr, "realpath(%s): %s\n", dev_device, strerror( errno ) );
                free( device );
                return E_DEVICE;
            }
            free(device);
            device = realpath_dev_device;
            debug( "trying to prepend '" DEVDIR
		   "' to device argument, now '%s'\n", device );
	    /* We need to lookup again in fstab: */
	    fstab_device = fstab_has_device( "/etc/fstab", device,
					     fstab_mntpt, NULL );
	    if( fstab_device ) {
	      do_umount_fstab( fstab_device, do_lazy, fstab_mntpt );
              free( device );
	      return E_EXECUMOUNT;
	    }
	}
    }

    /* does the device start with DEVDIR? */
    if( strncmp( device, DEVDIR, sizeof( DEVDIR )-1 ) != 0) {
        fprintf( stderr, _("Error: invalid device %s (must be in /dev/)\n"), device );
        free( device );
        return E_DEVICE;
    }

    /* check if we have a dmcrypt device */
    char *mapped_device;
    if( luks_get_mapped_device( device, &mapped_device ) ) {
        free(device);
        device = mapped_device;
        debug( "Unmounting mapped device %s instead.\n", device );
    }

    /* Now, we accept when devices have gone missing */
    if( check_umount_policy( device, 1 ) ) {
        free( device );
        return E_POLICY;
    }

    /* go for it */
    if( do_umount( device, do_lazy ) ) {
        free( device );
        return E_EXECUMOUNT;
    }

    /* release LUKS device, if appropriate */
    luks_release( device, 1 );
    free( device );

    /* delete mount point */
    remove_pmount_mntpt( mntpt );

    return 0;
}
