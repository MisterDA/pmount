/**
 * policy.c - functions for testing various policy parts for pmount
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2004 Canonical Ltd.
 *
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include "policy.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <mntent.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <libintl.h>
#include <sys/stat.h>
#include <sysfs/libsysfs.h>
#include <regex.h>

/*************************************************************************
 *
 * Sysfs query functions for determining if a device is removable
 *
 *************************************************************************/


/**
 * Check whether a bus occurs anywhere in the ancestry of a device.
 * @param dev sysfs device
 * @param buses NULL-terminated array of bus names to scan for
 * @return 0 if not found, 1 if found
 */
int
find_bus_ancestry( struct sysfs_device* dev, char** buses ) {
    char **i;

    if( !buses ) {
        debug ( "find_bus_ancestry: no buses to check, fail\n" );
        return 0;
    }

    if( !dev ) {
        debug ( "find_bus_ancestry: dev == NULL, fail\n" );
        return 0;
    }

    for( i = buses; *i; ++i ) {
        if( !strcmp( dev->bus, *i ) ) {
            debug ( "find_bus_ancestry: device %s (path %s, bus %s) matches query, success\n", 
                    dev->name, dev->path, dev->bus );
            return 1;
        }
    }

    debug ( "find_bus_ancestry: device %s (path %s, bus %s) does not match, trying parent\n", 
            dev->name, dev->path, dev->bus );
    return find_bus_ancestry( sysfs_get_device_parent( dev ), buses );
}

/**
 * Find sysfs node that matches the major and minor device number of the given
 * device. Exit the process immediately on errors.
 * @param dev device node to search for (e. g. /dev/sda1)
 * @param blockdevpath if not NULL, the corresponding /sys/block/<drive>/ path
 *        is written into this buffer; this can be used to query additional
 *        attributes
 * @param blockdevpathsize size of blockdevpath buffer (if not NULL)
 * @return Matching sysfs_device node (NULL if not found)
 */
struct sysfs_device*
find_sysfs_device( const char* dev, char* blockdevpath, size_t blockdevpathsize ) {
    unsigned char devmajor, devminor;
    unsigned char sysmajor, sysminor;
    char mntpath[PATH_MAX];
    char blockdirname[255];
    char devdirname[512]; // < 255 chars blockdir + max. 255 chars subdir
    char devfilename[PATH_MAX];
    char linkfilename[1024];
    DIR *devdir, *partdir;
    struct dirent *devdirent, *partdirent;
    struct sysfs_device *sysdev = NULL;
    struct stat devstat;
    
    /* determine major and minor of dev */
    if( stat( dev, &devstat ) ) {
        perror( _("Error: could not get status of device") );
        exit( -1 );
    }
    devmajor = (unsigned char) ( devstat.st_rdev >> 8 );
    devminor = (unsigned char) ( devstat.st_rdev & 255 );

    debug( "find_sysfs_device: looking for sysfs directory for device %u:%u\n",
                (unsigned) devmajor, (unsigned) devminor );

    /* get /sys/block/ */
    if( sysfs_get_mnt_path( mntpath, sizeof(mntpath) ) ) {
        fputs( _("Error: could not get sysfs directory\n"), stderr );
        exit( -1 );
    }
    snprintf( blockdirname, sizeof( blockdirname ), "%s/block/", mntpath );

    devdir = opendir( blockdirname );
    if( !devdir ) {
        perror( _("Error: could not open <sysfs dir>/block/") );
        exit( -1 );
    }

    /* open each subdirectory and see whether major device matches */
    while( ( devdirent = readdir( devdir ) ) != NULL ) {
        /* construct /sys/block/<device> */
        snprintf( devdirname, sizeof( devdirname ), "%s%s", blockdirname, devdirent->d_name );

        /* construct /sys/block/<device>/dev */
        snprintf( devfilename, sizeof( devfilename ), "%s%s", devdirname, "/dev" );

        /* read the block device major:minor */
        if( read_number_colon_number( devfilename, &sysmajor, &sysminor ) == -1 )
            continue;

        debug( "find_sysfs_device: checking whether %s is on %s (%u:%u)\n",
                    dev, devdirname, (unsigned) sysmajor, (unsigned) sysminor );

        if( sysmajor == devmajor ) {
            debug( "find_sysfs_device: major device numbers match\n");

            /* if dev is a partition, check that there is a subdir that matches
             * the partition */
            if( sysminor != devminor ) {
                int found_part = 0;
                
                debug( "find_sysfs_device: minor device numbers do not match, checking partitions...\n");

                partdir = opendir( devdirname );
                if( !partdir ) {
                    perror( _("Error: could not open <sysfs dir>/block/<device>/") );
                    exit( -1 );
                }
                while( ( partdirent = readdir( partdir ) ) != NULL ) {
                    if( partdirent->d_type != DT_DIR )
                        continue;

                    /* construct /sys/block/<device>/<partition>/dev */
                    snprintf( devfilename, sizeof( devfilename ), "%s/%s/%s",
                            devdirname, partdirent->d_name, "dev" );

                    /* read the block device major:minor */
                    if( read_number_colon_number( devfilename, &sysmajor, &sysminor ) == -1 )
                        continue;

                    debug( "find_sysfs_device: checking whether device %s matches partition %u:%u\n",
                                dev, (unsigned) sysmajor, (unsigned) sysminor );

                    if( sysmajor == devmajor && sysminor == devminor ) {
                        debug( "find_sysfs_device: -> partition matches, belongs to block device %s\n",
                                    devdirname );
                        found_part = 1;
                        break;
                    }
                } 

                closedir( partdir );

                if( !found_part ) {
                    /* dev is a partition, but it does not belong to the
                     * currently examined device; skip to next device */
                    continue;
                }
            } else
                debug( "find_sysfs_device: minor device numbers also match, %s is a raw device\n",
                            dev );


            snprintf( devfilename, sizeof( devfilename ), "%s/device", devdirname );

            /* read out the link */
            if( !sysfs_get_link( devfilename, linkfilename, 1024 ) )
                sysdev = sysfs_open_device_path( linkfilename );

            /* return /sys/block/<drive> if requested */
            if( blockdevpath )
                snprintf( blockdevpath, blockdevpathsize, "%s", devdirname );
            break;
        }
    }

    closedir( devdir );

    return sysdev;
}

/**
 * Return whether attribute attr in blockdevpath exists and has value '1'.
 */
int
get_blockdev_attr( const char* blockdevpath, const char* attr )
{
    char path[PATH_MAX];
    FILE* f;
    int result;
    char value;

    snprintf( path, sizeof( path ), "%s/%s", blockdevpath, attr );

    f = fopen( path, "r" );
    if( !f ) {
        debug( "get_blockdev_attr: could not open %s\n", path );
        return 0;
    }

    result = fread( &value, 1, 1, f );
    fclose( f );

    if( result != 1 ) {
        debug( "get_blockdev_attr: could not read %s\n", path );
        return 0;
    }

    debug( "get_blockdev_attr: value of %s == %c\n", path, value );

    return value == '1';
}

/*************************************************************************
 *
 * Policy functions
 *
 *************************************************************************/

int
device_valid( const char* device )
{
    struct stat st;

    if( stat( device, &st ) ) {
        fprintf( stderr, _("Error: device %s does not exist\n"), device );
        return 0;
    }

    if( !S_ISBLK( st.st_mode ) ) {
        fprintf( stderr, _("Error: %s is not a block device\n"), device );
        return 0;
    }

    return 1;
}

const char*
fstab_has_device( const char* fname, const char* device, char* mntpt, int *uid )
{
    FILE* f;
    struct mntent *entry;
    char pathbuf[PATH_MAX];
    static char fstab_device[PATH_MAX];
    char* realdev;
    char* uidopt;

    if( !( f = fopen( fname, "r" ) ) ) {
        perror( _("Error: could not open fstab-type file") );
        exit( 100 );
    }

    while( ( entry = getmntent( f ) ) != NULL ) {
        snprintf( fstab_device, sizeof( fstab_device ), "%s", entry->mnt_fsname );

        if( realpath( fstab_device, pathbuf ) )
            realdev = pathbuf;
        else
            realdev = fstab_device;

        if( !strcmp( realdev, device ) ) {
                endmntent( f );
                if( mntpt ) {
                    snprintf( mntpt, MEDIA_STRING_SIZE-1, "%s", entry->mnt_dir );
                }
                if( uid ) {
                    uidopt = hasmntopt( entry, "uid" );
                    if( uidopt )
                        uidopt = strchr( uidopt, '=' );
                    if( uidopt ) {
                        ++uidopt; /* skip the '=' */
                        /* FIXME: this probably needs more checking */
                        *uid = atoi( uidopt );
                    } else
                        *uid = -1;
                }
                return fstab_device;
        }
    }

    /* just for safety */
    if( mntpt )
        *mntpt = 0; 

    endmntent( f );
    return NULL;
}

int
fstab_has_mntpt( const char* fname, const char* mntpt, char* device, size_t device_size )
{
    FILE* f;
    struct mntent *entry;
    char realmntptbuf[PATH_MAX];
    char fstabmntptbuf[PATH_MAX];
    const char* realmntpt, *fstabmntpt;

    /* resolve symlinks, if possible */
    if( realpath( mntpt, realmntptbuf ) )
        realmntpt = realmntptbuf;
    else
        realmntpt = mntpt;

    if( !( f = fopen( fname, "r" ) ) ) {
        perror( _("Error: could not open fstab-type file") );
        exit( 100 );
    }

    while( ( entry = getmntent( f ) ) != NULL ) {
        /* resolve symlinks, if possible */
        if( realpath( entry->mnt_dir, fstabmntptbuf ) )
            fstabmntpt = fstabmntptbuf;
        else
            fstabmntpt = entry->mnt_dir;

        if( !strcmp( fstabmntpt, realmntpt ) ) {
            if( device )
                snprintf( device, device_size, "%s", entry->mnt_fsname );
            endmntent( f );
            return 1;
        }
    }

    endmntent( f );
    return 0;
}

int
device_mounted( const char* device, int expect, char* mntpt )
{
    char mp[MEDIA_STRING_SIZE];
    int uid;
    int mounted = fstab_has_device( "/etc/mtab", device, mp, &uid ) ||
                  fstab_has_device( "/proc/mounts", device, mp, &uid );
    if( mounted && !expect )
        fprintf( stderr, _("Error: device %s is already mounted to %s\n"), device, mp );
    else if( !mounted && expect )
        fprintf( stderr, _("Error: device %s is not mounted\n"), device );
    if( mounted && expect && uid >= 0 && (uid_t) uid != getuid() && getuid() > 0) {
        fprintf( stderr, _("Error: device %s was not mounted by you\n"), device );
        return 0;
    }

    if( mntpt )
        snprintf( mntpt, MEDIA_STRING_SIZE-1, "%s", mp );

    return mounted;
}

int
device_removable( const char* device )
{
    struct sysfs_device *dev;
    static char* hotplug_buses[] = { "usb", "ieee1394", "mmc", NULL };
    int removable;
    char blockdevpath[PATH_MAX];

    dev = find_sysfs_device( device, blockdevpath, sizeof( blockdevpath ) );
    if( !dev ) {
        debug( "device_removable: could not find a sysfs device for %s\n", device );
        return 0; 
    }

    debug( "device_removable: corresponding block device for %s is %s\n",
                device, blockdevpath );

    /* check whether device has "removable" attribute with value '1' */
    removable = get_blockdev_attr( blockdevpath, "removable" );

    /* if not, fall back to bus scanning (regard USB and FireWire as removable) */
    if( !removable )
        removable = find_bus_ancestry( dev, hotplug_buses );
    sysfs_close_device( dev );

    if( !removable )
        fprintf( stderr, _("Error: device %s is not removable\n"), device );
    
    return removable;
}

int
device_whitelisted( const char* device )
{
    FILE* fwl;
    char line[1024];
    char *d;
    regex_t re;
    regmatch_t match[3];
    int result;
    const char* whitelist_regex = "^[[:space:]]*([a-zA-Z0-9/_+.-]+)[[:space:]]*(#.*)?$";

    fwl = fopen( WHITELIST, "r" );
    if( !fwl )
        return 0;

    result = regcomp( &re, whitelist_regex, REG_EXTENDED );
    if( result ) {
        regerror( result, &re, line, sizeof( line ) );
        fprintf( stderr, "Internal error: device_whitelisted(): could not compile regex: %s\n", line );
        exit( -1 );
    }

    debug( "device_whitelist: checking " WHITELIST "...\n" );

    while( fgets( line, sizeof( line ), fwl ) ) {
        /* ignore lines which are too long */
        if( strlen( line ) == sizeof( line ) - 1 ) {
            debug ("ignoring invalid oversized line\n");
            continue;
        }

       if (!regexec (&re, line, 3, match, 0)) {
           line[match[1].rm_eo] = 0;
           d = line+match[1].rm_so;
           debug( "comparing %s against whitelisted '%s'\n", device, d);
           if( !strcmp( d, device ) ) {
               debug( "device_whitlisted(): match, returning 1\n" );
               fclose( fwl );
               return 1;
           }
       }
    }

    fclose( fwl );
   debug( "device_whitlisted(): nothing matched, returning 0\n" );
    return 0;
}

int 
device_locked( const char* device )
{
    char lockdirname[PATH_MAX];
    int locked;

    make_lockdir_name( device, lockdirname, sizeof( lockdirname ) );
    locked = is_dir( lockdirname );

    if( locked )
        fprintf( stderr, _("Error: device %s is locked\n"), device );

    return locked;
}

int
mntpt_valid( const char* mntpt ) 
{
    if( fstab_has_mntpt( "/etc/fstab", mntpt, NULL, 0 ) ) {
	fprintf( stderr, _("Error: mount point %s is already in /etc/fstab\n"), mntpt );
	return 0;
    }
    return !assert_dir( mntpt, 1 ) && !assert_emptydir( mntpt );
}

int
mntpt_mounted( const char* mntpt, int expect )
{
    int mounted = fstab_has_mntpt( "/etc/mtab", mntpt, NULL, 0 ) ||
                  fstab_has_mntpt( "/proc/mounts", mntpt, NULL, 0 );

    if( mounted && !expect )
        fprintf( stderr, _("Error: directory %s already contains a mounted file system\n"), mntpt );
    else if( !mounted && expect )
        fprintf( stderr, _("Error: directory %s does not contain a mounted file system\n"), mntpt );

    return mounted;
}

void make_lockdir_name( const char* device, char* name, size_t name_size )
{
    char* devname;

    devname = strreplace( device, '/', '_' );
    snprintf( name, name_size, "%s%s", LOCKDIR, devname );
    free( devname );
}

