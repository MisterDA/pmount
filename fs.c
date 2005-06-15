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
 * to command line options.
 */
static struct FS supported_fs[] = {
    { "udf", "rw,nosuid,exec,nodev,user", 1, "007", 1 },
    { "iso9660", "rw,nosuid,exec,nodev,user", 1, NULL, 0 },
    { "vfat", "rw,nosuid,exec,nodev,user,quiet", 1, "077", 1 },
    { "ntfs", "ro,nosuid,exec,nodev,user", 1, "077", 1 },
    { "hfsplus", "rw,nosuid,exec,nodev,user", 1, NULL, 0 },
    { "hfs", "rw,nosuid,exec,nodev,user", 1, NULL, 0 },
    { "ext3", "rw,nodev,exec,noauto,nosuid,user", 0, NULL, 0 },
    { "ext2", "rw,nodev,exec,noauto,nosuid,user", 0, NULL, 0 },
    { "reiserfs", "rw,nodev,exec,noauto,nosuid,user", 0, NULL, 0 },
    { "xfs", "rw,nodev,exec,noauto,nosuid,user", 0, NULL, 0 },
    { "jfs", "rw,nodev,exec,noauto,nosuid,user", 0, NULL, 0 },
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
