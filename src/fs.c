/**
 * fs.c - data type and interface function for supported file systems
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2004 Canonical Ltd.
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include "fs.h"

#include <string.h>

/** 
 * List of file systems supported by pmount; terminated with a struct with
 * fsname == NULL;
 * Do not specify the 'sync' option; this will be dynamically added according
 * to command line options. It is anyway better not to use it...
 */
static struct FS supported_fs[] = {
    {
	.fsname = "udf",
	.options = "nosuid,nodev,user",
	.support_ugid = 1,
	.umask = "000",
	.iocharset_format = ",iocharset=%s",
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "iso9660",
	.options = "nosuid,nodev,user",
	.support_ugid = 1,
	.umask = NULL,
	.iocharset_format = ",iocharset=%s",
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "vfat",
	.options = "nosuid,nodev,user,quiet,shortname=mixed",
	.support_ugid = 1,
	.umask = "077",
	.iocharset_format = ",iocharset=%s",
	.fdmask = ",fmask=%04o,dmask=%04o",
	.skip_autodetect = 0,
    },
    {
	.fsname = "exfat",
	.options = "nosuid,nodev,user,quiet,nonempty",
	.support_ugid = 1,
	.umask = "077",
	.iocharset_format = ",iocharset=%s",
	.fdmask = ",fmask=%04o,dmask=%04o",
	.skip_autodetect = 0,
    },
    {
	.fsname = "hfsplus",
	.options = "nosuid,nodev,user",
	.support_ugid = 1,
	.umask = NULL,
	.iocharset_format = NULL,
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "hfs",
	.options = "nosuid,nodev,user",
	.support_ugid = 1,
	.umask = "077",
	.iocharset_format = NULL,
	.fdmask = ",file_umask=%04o,dir_umask=%04o",
	.skip_autodetect = 0,
    },
    {
	.fsname = "ext3",
	.options = "nodev,noauto,nosuid,user,errors=remount-ro",
	.support_ugid = 0,
	.umask = NULL,
	.iocharset_format = NULL,
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "ext2",
	.options = "nodev,noauto,nosuid,user,errors=remount-ro",
	.support_ugid = 0,
	.umask = NULL,
	.iocharset_format = NULL,
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "ext4",
	.options = "nodev,noauto,nosuid,user,errors=remount-ro",
	.support_ugid = 0,
	.umask = NULL,
	.iocharset_format = NULL,
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "reiserfs",
	.options = "nodev,noauto,nosuid,user",
	.support_ugid = 0,
	.umask = NULL,
	.iocharset_format = NULL,
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "reiser4",
	.options = "nodev,noauto,nosuid,user",
	.support_ugid = 0,
	.umask = NULL,
	.iocharset_format = NULL,
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "xfs",
	.options = "nodev,noauto,nosuid,user",
	.support_ugid = 0,
	.umask = NULL,
	.iocharset_format = NULL,
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "jfs",
	.options = "nodev,noauto,nosuid,user,errors=remount-ro",
	.support_ugid = 0,
	.umask = NULL,
	.iocharset_format = ",iocharset=%s",
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "omfs",
	.options = "nodev,noauto,nosuid,user",
	.support_ugid = 0,
	.umask = NULL,
	.iocharset_format = NULL,
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = "ntfs-fuse",
	.options = "nosuid,nodev,user",
	.support_ugid = 1,
	.umask = "077",
	.iocharset_format = NULL, /* no nls support, it seems*/
	.fdmask = ",fmask=%04o,dmask=%04o",
	.skip_autodetect = 1, /* skip detection */
    },
    {
	.fsname = "ntfs-3g",
	.options = "nosuid,nodev,user",
	.support_ugid = 1,
	.umask = "077",
	.iocharset_format = NULL, /* no nls support, it seems*/
	.fdmask = ",fmask=%04o,dmask=%04o",
	.skip_autodetect = 1, /* skip detection */
    },
    {
	.fsname = "ntfs",
	.options = "nosuid,nodev,user",
	.support_ugid = 1,
	.umask = "077",
	.iocharset_format = ",nls=%s",
	.fdmask = NULL,
	.skip_autodetect = 0,
    },
    {
	.fsname = NULL,
    }
};

const struct FS*
get_supported_fs() {
    return supported_fs;
}

const struct FS*
get_fs_info( const char* fsname ) {
    struct FS* i;

    for( i = supported_fs; i->fsname; ++i )
        if( !strcmp( i->fsname, fsname ) )
            return i;
    return NULL;
}
