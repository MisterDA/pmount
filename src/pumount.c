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

/* error codes */
const int E_ARGS = 1;
const int E_DEVICE = 2;
const int E_POLICY = 4;
const int E_EXECUMOUNT = 5;
const int E_INTERNAL = 100;

static char mntpt[MEDIA_STRING_SIZE];

/**
 * Print some help.
 * @param exename Name of the executable (argv[0]).
 */
void
usage( const char* exename )
{
    printf( _("Usage:\n\n%s [options] <device>\n"
    "  Umount <device> from a directory below %s if policy requirements\n"
    "  are met (see pumount(1) for details). The mount point directory is removed\n"
    "  afterwards.\n\n"
    "Options:\n"
    "  -l, --lazy : umount lazily, see umount(8)\n"
    "  -d, --debug : enable debug output (very verbose)\n"
    "  -h, --help  : print help message and exit successfuly\n"),
        exename, MEDIADIR );
}

/**
 * Check whether the user is allowed to umount the given device.
 * @param do_lazy Flag if a lazy unmount is requested (in this case the device
 *        node does not need to exist)
 * @return 0 on success, -1 on failure
 */
int
check_umount_policy( const char* device, int do_lazy ) 
{
    int devvalid;

    devvalid = ( do_lazy || device_valid( device ) ) &&
        device_mounted( device, 1, mntpt );

    if( !devvalid )
        return -1;

    /* paranoid check */
    if( !mntpt || !*mntpt ) {
        fputs( _("Internal error: could not determine mount point\n"), stderr );
        exit( E_INTERNAL );
    }

    /* mount point must be below MEDIADIR */
    if( strncmp( mntpt, MEDIADIR, sizeof( MEDIADIR )-1 ) ) {
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
void
do_umount_fstab( const char* device, int lazy )
{
    /* drop all privileges */
    get_root();
    if( setuid( getuid() ) ) {
        perror( _("Error: could not drop all uid privileges") );
        return;
    }

    debug( "device %s handled by fstab, calling umount\n", device );

    if( lazy )
        execl( UMOUNTPROG, UMOUNTPROG, "-l", device, NULL );
    else
        execl( UMOUNTPROG, UMOUNTPROG, device, NULL );
    perror( _("Error: could not execute umount") );
}

/**
 * Raise to full privileges and mounts device to mntpt.
 * @param lazy 0 for normal umount, 1 for lazy umount
 * @return 0 on success, -1 if UMOUNTPROG could not be executed.
 */
int
do_umount( const char* device, int do_lazy )
{
    int status;

    if( do_lazy )
        status = spawnl( SPAWN_EROOT|SPAWN_RROOT, UMOUNTPROG, UMOUNTPROG, "-l",
                device, NULL );
    else
        status = spawnl( SPAWN_EROOT|SPAWN_RROOT, UMOUNTPROG, UMOUNTPROG,
                device, NULL );

    if( status != 0 ) {
        fprintf( stderr, _("Error: umount failed\n") );
        return -1;
    }

    return 0;
}

/**
 * Entry point.
 */
int
main( int argc, char** argv )
{
    char device[PATH_MAX], mntptdev[PATH_MAX], path[PATH_MAX];
    const char* fstab_device;
    int is_real_path = 0;
    int do_lazy = 0;

    int  option;
    static struct option long_opts[] = {
        { "help", 0, NULL, 'h'},
        { "debug", 0, NULL, 'd'},
        { "lazy", 0, NULL, 'l'},
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
        switch( option = getopt_long( argc, argv, "+hdlu", long_opts, NULL ) ) {
            case -1:        break;          /* end of arguments */
            case '?':        return E_ARGS;  /* unknown argument */

            case 'h':        usage( argv[0] ); return 0;

            case 'd':   enable_debug = 1; break;

            case 'l':        do_lazy = 1; break;

            default:
                fprintf( stderr, _("Internal error: getopt_long() returned unknown value\n") );
                return E_INTERNAL;
        }
    } while( option != -1 );

    /* invalid number of args? */
    if( optind + 1 != argc ) {
        usage( argv[0] );
        return E_ARGS;
    }

    /* if we got a mount point, convert it to a device */
    debug ("checking whether %s is a mounted directory\n", argv[optind]);
    if( fstab_has_mntpt( "/proc/mounts", argv[optind], mntptdev, sizeof(mntptdev) ) ) {
        debug( "resolved mount point %s to device %s\n", argv[optind], mntptdev );
        argv[optind] = mntptdev;
    } else if( !strchr( argv[optind], '/' ) ) {
        /* try to prepend MEDIADIR */
        snprintf( path, sizeof( path ), "%s%s", MEDIADIR, argv[optind] );
        debug ("checking whether %s is a mounted directory\n", path);
        if( fstab_has_mntpt( "/proc/mounts", path, mntptdev, sizeof(mntptdev) ) ) {
            debug( "resolved mount point %s to device %s\n", path, mntptdev );
            argv[optind] = mntptdev;
        }
    }

    /* get real path, if possible */
    if( realpath( argv[optind], device ) ) {
        debug( "resolved %s to device %s\n", argv[optind], device );
        is_real_path = 1;
    } else {
        debug( "%s cannot be resolved to a proper device node\n", argv[optind] );
        snprintf( device, sizeof( device ), "%s", argv[optind] );
    }

    /* is the device already handled by fstab? */
    fstab_device = fstab_has_device( "/etc/fstab", device, NULL, NULL );
    if( fstab_device ) {
        do_umount_fstab( fstab_device, do_lazy );
        return E_EXECUMOUNT;
    }

    /* we cannot really check the real path when unmounting lazily since the
     * device node might not exist any more */
    if( !is_real_path && !do_lazy ) {
        perror( _("Error: could not determine real path of the device") );
        return E_DEVICE;
    }

    /* does the device start with DEVDIR? */
    if( strncmp( device, DEVDIR, sizeof( DEVDIR )-1 ) ) {
        fprintf( stderr, _("Error: invalid device %s (must be in /dev/)\n"), device );
        return E_DEVICE;
    }

    /* check if we have a dmcrypt device */
    if( luks_get_mapped_device( device, device, sizeof( device ) ) )
        debug( "Unmounting mapped device %s instead.\n", device );

    if( check_umount_policy( device, do_lazy ) )
        return E_POLICY;

    /* go for it */
    if( do_umount( device, do_lazy ) )
        return E_EXECUMOUNT;

    /* release LUKS device, if appropriate */
    luks_release( device );

    /* delete mount point */
    remove_pmount_mntpt( mntpt );

    return 0; 
}
