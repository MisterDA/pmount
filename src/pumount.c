/* -*- c-basic-offset: 4; -*- */
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
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

#include "policy.h"
#include "utils.h"
#include "luks.h"
#include "conf.h"
#include "config.h"

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
    "  -l, --lazy    : umount lazily, see umount(8)\n"
    "  --luks-force  : luksClose devices pmount didn't open\n"
    "  -D            : umount all partitions of the device, then stops it for safe removal\n"
    "  -d, --debug   : enable debug output (very verbose)\n"
    "  -h, --help    : print help message and exit successfuly\n"
    "  -V, --version : print version number and exit successfully\n"),
        exename, MEDIADIR );
}

/**
 * Check whether the user is allowed to umount the given device.
 * @param ok_if_inexistant whether it is allowed for the device to not
 *        exist, as should be the case when the device has gone
 *        missing for some reason
 * @return 0 on success, -1 on failure
 */
int
check_umount_policy( const char* device, int ok_if_inexistant ) 
{
    int devvalid;
    char mediadir[PATH_MAX];
    
    devvalid = ( ok_if_inexistant || device_valid( device ) ) &&
        device_mounted( device, 1, mntpt );

    if( !devvalid )
        return -1;

    /* paranoid check */
    if( !mntpt || !*mntpt ) {
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
        /* check CONF_FILE, it might be okay */
        char *o_mntpt = NULL;
        int passed = 0;
        if( !get_conf_for_device( device, NULL, NULL, NULL, &o_mntpt, NULL ) ) {
            if( NULL != o_mntpt ) {
                if( !strcmp( mntpt, o_mntpt ) ) {
                    debug( "mount point allowed from config: %s\n", mntpt );
                    passed = 1;
                }
                free( o_mntpt );
            }
        }
        if( !passed ) {
            fprintf( stderr, _("Error: mount point %s is not below %s\n"), mntpt,
                    MEDIADIR );
            return -1;
        }
    }

    debug( "policy check passed\n" );
    return 0;
}

/**
 * Drop all privileges and exec 'umount device'.
 * @param lazy 0 for normal umount, 1 for lazy umount
 * @return 0 on success, E_EXECUMOUNT if UMOUNTPROG could not be executed.
 */
int
do_umount_fstab( const char* device, int lazy, const char * fstab_mntpt )
{
    int status;
    
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
        status = spawnl( 0, UMOUNTPROG, UMOUNTPROG, "-l", device, NULL );
    else
        status = spawnl( 0, UMOUNTPROG, UMOUNTPROG, device, NULL );
    
    if( status != 0 ) {
        perror( _("Error: could not execute umount") );
        return E_EXECUMOUNT;
    }
    
    return 0;
}

/**
 * Raise to full privileges and mounts device to mntpt.
 * @param device full device name
 * @param do_lazy 0 for normal umount, 1 for lazy umount
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

int
umount_device( const char* device, size_t devicesize, const char* mntpt,
        int do_lazy, int full_device )
{
    const char* fstab_device;
    char fstab_mntpt[MEDIA_STRING_SIZE];

    /* in full device mode, we need to check is the device is handled by fstab */
    if( full_device ) {
        fstab_device = fstab_has_device( "/etc/fstab", device, fstab_mntpt, NULL );
        if( fstab_device && device_mounted( device, 1, NULL ) ) {
            return do_umount_fstab( fstab_device, do_lazy, fstab_mntpt );
        }
    /* in regular mode, we check if we have a dmcrypt device */
    } else if( luks_get_mapped_device( device, (char *) device, devicesize ) ) {
        debug( "Unmounting mapped device %s instead.\n", device );
    }

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

/**
 * Entry point.
 *
 */
int
main( int argc, char** argv )
{
    char device[PATH_MAX], mntptdev[PATH_MAX], path[PATH_MAX];
    const char* fstab_device;
    char fstab_mntpt[MEDIA_STRING_SIZE]; 
    int is_real_path = 0;
    int do_lazy = 0;
    int luks_force = 0;
    int full_device = 0;
    
    int error_occurred = 0;
    int result;

    int  option;
    static struct option long_opts[] = {
        { "help", 0, NULL, 'h'},
        { "debug", 0, NULL, 'd'},
        { "lazy", 0, NULL, 'l'},
        { "yes-I-really-want-lazy-unmount", 0, NULL, 'R'},
        { "luks-force", 0, NULL, 'L'},
        { "full-device", 0, NULL, 'D'},
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
        switch( option = getopt_long( argc, argv, "+hdluDV", long_opts, NULL ) ) {
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

            case 'L':        luks_force = 1; break;
            
            case 'D':   full_device = 1; break;

            case 'V': puts(VERSION); return 0;

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

    /* in full_device mode, we'll deal with all partitions anyways */
    if( !full_device ) {
        /* is the device already handled by fstab? */
        fstab_device = fstab_has_device( "/etc/fstab", device, fstab_mntpt, NULL );
        if( fstab_device ) {
            if( device_mounted( device, 1, NULL ) ) {
                return do_umount_fstab( fstab_device, do_lazy, fstab_mntpt );
            } else {
                return 0;
            }
        }
    }

    /* we cannot really check the real path when unmounting lazily since the
     * device node might not exist any more */
    if( !is_real_path && !do_lazy ) {
        /* try to prepend '/dev' */
        if( strncmp( device, DEVDIR, sizeof( DEVDIR )-1 ) ) { 
            char d[PATH_MAX];
            snprintf( d, sizeof( d ), "%s%s", DEVDIR, device );
            if ( !realpath( d, device ) ) {
                perror( _("Error: could not determine real path of the device") );
                return E_DEVICE;
            }
            debug( "trying to prepend '" DEVDIR 
		   "' to device argument, now '%s'\n", device );
            if( !full_device ) {
                /* We need to lookup again in fstab: */
                fstab_device = fstab_has_device( "/etc/fstab", device, 
                                                 fstab_mntpt, NULL );
                if( fstab_device ) {
                  if( device_mounted( device, 1, NULL ) ) {
                      return do_umount_fstab( fstab_device, do_lazy, fstab_mntpt );
                  } else {
                      return 0;
                  }
                }
            }
	}
    }

    /* does the device start with DEVDIR? */
    if( strncmp( device, DEVDIR, sizeof( DEVDIR )-1 ) ) {
        fprintf( stderr, _("Error: invalid device %s (must be in /dev/)\n"), device );
        return E_DEVICE;
    }
    
    /* we need to get the full device name (e.g. /dev/sde), get list of all its
     partitions, and try to unmount them all... */
    if( full_device ) {
        char devdirname[MEDIA_STRING_SIZE];
        if( !find_sysfs_device( device, devdirname, MEDIA_STRING_SIZE) ) {
            fprintf( stderr, _("Warning: unable to find device path for %s, "
                    "full-device mode disabled\n"),
                    device );
            full_device = 0;
        } else {
            debug( "device path for %s is %s\n", device, devdirname );
            
            DIR *partdir;
            struct dirent *partdirent;
            char partdirname[MEDIA_STRING_SIZE];
            struct stat stat_info;
            
            partdir = opendir( devdirname );
            if( !partdir ) {
                perror( _("Error: could not open <sysfs dir>/block/<device>/") );
                exit( -1 );
            }
            while( ( partdirent = readdir( partdir ) ) != NULL ) {
                if( partdirent->d_type != DT_DIR
                        || !strcmp( partdirent->d_name, "." )
                        || !strcmp( partdirent->d_name, ".." ) )
                    continue;
                
                /* construct /sys/block/<device>/<partition>/dev */
                snprintf( partdirname, sizeof( partdirname ), "%s/%s/%s",
                        devdirname, partdirent->d_name, "dev" );

                /* make sure it is a device, i.e has a file dev */
                if( 0 != stat( partdirname, &stat_info ) ) {
                    /* ENOENT (does not exist) is "okay" we just ignore this one */
                    if( ENOENT != errno ) {
                        perror( _("Error: could not stat <sysfs dir>/block/<device>/<part>/dev") );
                        exit( -1 );
                    }
                    continue;
                }
                /* must be a file */
                if( !S_ISREG( stat_info.st_mode ) ) {
                    continue;
                }

                /* construct /dev/<partition> */
                snprintf( device, sizeof( device ), "%s%s", DEVDIR, partdirent->d_name );
                debug( "processing found partition: %s\n", device );
                
                /* check if we have a dmcrypt device */
                if( luks_get_mapped_device( device, device, sizeof( device ) ) )
                    debug( "Using mapped device %s instead.\n", device );

                if( device_mounted( device, 1, mntpt ) ) {
                    debug( "device %s mounted, unmounting\n", device );
                    result = umount_device( device, sizeof( device ), mntpt,
                            do_lazy, full_device );
                    if( result != 0 ) {
                        fprintf( stderr, _("Failed to umount device %s : error %d\n"),
                                device, result );
                        error_occurred = -1;
                    } else {
                        printf( _("Device %s umounted\n"), device );
                    }
                }
            }
            closedir( partdir );
            
            /* no errors: let's stop the device completely, for safe removal */
            if ( !error_occurred ) {
                char *c;
                FILE *f;

                /* flush buffers */
                sync();
                
                /* resolve devdirname (<sysfs>/block/<device> to something like:
                 * /sys/devices/pci0000:00/0000:00:06.0/usb1/1-2/1-2:1.0/host5/target5:0:0/5:0:0:0/block/sdd */
                if( !realpath( devdirname, device ) ) {
                    debug( "unable to resolve %s\n", device );
                    goto err_stop;
                }
                debug( "device %s resolved to %s\n", devdirname, device );
                
                /* now extract the part we want, up to the grand-parent of the host
                 e.g: /sys/devices/pci0000:00/0000:00:06.0/usb1/1-2 */
                while( c = strrchr( device, '/' ) ) {
                    /* end the string there, to move back */
                    *c = 0;
                    /* found the host part? */
                    if( !strncmp( c + 1, "host", 4 ) ) {
                        break;
                    }
                }
                if( c == NULL ) {
                    debug( "unable to find host for %s\n", device );
                    goto err_stop;
                }
                /* we need to move back one more time */
                if( NULL == ( c = strrchr( device, '/' ) ) ) {
                    debug( "cannot move back one last time in %s\n", device );
                    goto err_stop;
                }
                /* end the string there */
                *c = 0;
                debug( "full name is %s\n", device );
                /* now we need the last component, aka the bus id */
                if( NULL == ( c = strrchr( device, '/' ) ) ) {
                    debug( "cannot extract last component of %s\n", device );
                    goto err_stop;
                }
                /* move up, so this points to the name only, e.g. 1-2 */
                ++c;
                
                /* unbind driver: write the bus id to <device>/driver/unbind */
                snprintf( path, sizeof( path ), "%s/driver/unbind", device );
                if ( root_write_to_file( path, c ) ) {
                    goto err_stop;
                }
                
                /* suspend device. step 1: write "0" to <device>/power/autosuspend */
                snprintf( path, sizeof( path ), "%s/power/autosuspend", device );
                if ( root_write_to_file( path, "0" ) ) {
                    goto err_stop;
                }
                /* step 2: write "auto" to <device>/power/control */
                snprintf( path, sizeof( path ), "%s/power/control", device );
                if ( root_write_to_file( path, "auto" ) ) {
                    goto err_stop;
                }
                
                c = strrchr( devdirname, '/' );
                printf( _("Device %s%s stopped, you should now be able to safely unplug it\n"),
                        DEVDIR, c + 1);
            }
            
            return error_occurred;

err_stop:
            fputs( _("Error: Unable to stop device\n"), stderr );
            return -1;
        }
    }

    return umount_device( device, sizeof( device ), mntpt, do_lazy, full_device );
}
