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
    { "udf", "nosuid,nodev,user", 1, "000", ",iocharset=%s" },
    { "iso9660", "nosuid,nodev,user", 1, NULL, ",iocharset=%s" },
    { "vfat", "nosuid,nodev,user,quiet,shortname=mixed", 1, "077", 
      ",iocharset=%s",",fmask=%04o,dmask=%04o"},
    { "hfsplus", "nosuid,nodev,user", 1, NULL, 0 },
    { "hfs", "nosuid,nodev,user", 1, "077", NULL, 
      ",file_umask=%04o,dir_umask=%04o"},
    { "ext3", "nodev,noauto,nosuid,user,errors=remount-ro", 0, NULL, 0 },
    { "ext2", "nodev,noauto,nosuid,user,errors=remount-ro", 0, NULL, 0 },
    { "ext4", "nodev,noauto,nosuid,user,errors=remount-ro", 0, NULL, 0 },
    { "reiserfs", "nodev,noauto,nosuid,user", 0, NULL, 0 },
    { "reiser4", "nodev,noauto,nosuid,user", 0, NULL, 0 },
    { "xfs", "nodev,noauto,nosuid,user", 0, NULL, 0 },
    { "jfs", "nodev,noauto,nosuid,user,errors=remount-ro", 0, NULL, 
      ",iocharset=%s"},
    { "omfs", "nodev,noauto,nosuid,user", 0, NULL, 0 },
    { "ntfs-fuse", "nosuid,nodev,user", 1, "077", 
      NULL /* no nls support, it seems*/, ",fmask=%04o,dmask=%04o", 
      1 /* skip detection */
    },
    { "ntfs-3g", "nosuid,nodev,user", 1, "077", 
      NULL /* no nls support, it seems*/, ",fmask=%04o,dmask=%04o", 
      1 /* skip detection */
    },
    { "ntfs", "nosuid,nodev,user", 1, "077", ",nls=%s" },
    { NULL, NULL, 0, NULL, 0}
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
