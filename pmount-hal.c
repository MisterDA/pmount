/**
 * pmount-hal.c - pmount wrapper that uses HAL to get additional information
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2005 Canonical Ltd.
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include <stdio.h>
#include <stdlib.h> /* calloc */
#include <limits.h> /* PATH_MAX */
#include <unistd.h> /* execvp */
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <locale.h>
#include <libintl.h>
#include <libhal.h>

#include "policy.h"

/* gettext abbreviation */
#define _(String) gettext(String)

void help() {
    puts( _(
"pmount-hal - execute pmount with additional information from hal\n\n"
"Usage: pmount-hal <hal UDI> [pmount options]\n\n"
"This command mounts the device described by the given UDI using pmount. The\n"
"file system type, the volume storage policy and the desired label will be\n"
"read out from hal and passed to pmount."));
}

int empty_dir( const char* dirname )
{
    DIR* dir;
    struct dirent* dirent;

    if( !( dir = opendir( dirname ) ) )
        return 1;

    while ( ( dirent = readdir( dir ) ) ) {
        if( strcmp( dirent->d_name, "." ) && 
            strcmp( dirent->d_name, ".." ) ) {
            closedir( dir );
            return 0;
        }
    }

    closedir( dir );
    return 1;
}

/* Return 0 if mntpt is nonempty and/or already mounted, 1 otherwise. */
int valid_mntpt( const char* mntpt )
{
    struct stat st;
    char realp[PATH_MAX];

    if( stat( mntpt, &st ) )
        return 1; /* does not exist */

    /* resolve symlinks */
    if( S_ISLNK( st.st_mode ) ) {
        if( !realpath( mntpt, realp ) )
            return 0;
        mntpt = realp;
        if( stat( mntpt, &st ) )
            return 0; /* invalid link */
    }
    
    if( !S_ISDIR( st.st_mode ) )
        return 0;

    if( !empty_dir( mntpt ) )
        return 0;

    if( fstab_has_mntpt( "/proc/mounts", mntpt, NULL, 0 ) ||
        fstab_has_mntpt( "/etc/mtab", mntpt, NULL, 0 ) )
        return 0;

    return 1;
}

void get_free_label( const char* label, char* result, size_t result_size )
{
    char mntpt[PATH_MAX];
    snprintf( mntpt, sizeof(mntpt), "/media/%s", label );
    int n;

    if( valid_mntpt( mntpt ) ) {
        strncpy( result, label, result_size-1 );
        result[result_size-1] = 0;
        return;
    }

    for( n = 1; n > 0; ++n ) {
        snprintf( mntpt, sizeof(mntpt), "/media/%s-%i", label, n );
        if( valid_mntpt( mntpt ) ) {
            snprintf( result, result_size, "%s-%i", label, n );
            return;
        }
    }
}

void exec_pmount( const char* device, const char* fstype, const char* label,
        dbus_bool_t sync, dbus_bool_t noatime, int addargc, const char* const* addargv ) 
{
    const char** argv = (const char**) calloc( sizeof( const char* ), addargc+10 );
    int argc = 0;
    char freelabel[PATH_MAX];
    int i;

    argv[argc++] = "pmount";

    if( fstype ) {
        argv[argc++] = "-t";
        argv[argc++] = fstype;
    }

    if( !sync )
        argv[argc++] = "--async";
    if( noatime )
        argv[argc++] = "--noatime";

    for( i = 0; i < addargc; ++i )
        argv[argc++] = addargv[i];

    argv[argc++] = device;
    if( label ) {
        get_free_label( label, freelabel, sizeof( freelabel ) );
        argv[argc++] = freelabel;
    }
    argv[argc] = NULL;

    execvp( argv[0], (char *const *) argv );
    fputs( _("Error: could not execute pmount\n"), stderr );
    exit( 1 );
}

int
main( int argc, const char** argv ) 
{
    LibHalContext *hal_ctx;
    dbus_bool_t sync = TRUE, noatime = FALSE;
    const char* udi;
    char* device = NULL;
    char* label = NULL;
    char* fstype = NULL;

    /* initialize locale */
    setlocale( LC_ALL, "" );
    bindtextdomain( "pmount", NULL );
    textdomain( "pmount" );

    if( argc < 2 ) {
        help();
        return 0;
    }

    udi = argv[1];

    /* initialize hal connection */
    hal_ctx = hal_initialize( NULL, FALSE );
    if( !hal_ctx ) {
        fprintf( stderr, _("Error: could not connect to hal\n") );
        return 1;
    }

    /* get all interesting properties */
    if( !hal_device_exists( hal_ctx, udi ) ) {
        fprintf( stderr, _("Error: given UDI does not exist\n") );
        return 1;
    }

    if( hal_device_property_exists( hal_ctx, udi, "block.device" ) )
        device = hal_device_get_property_string( hal_ctx, udi, "block.device" );
    if( !device ) {
        fprintf( stderr, _("Error: given UDI is not a mountable volume\n") );
        return 1;
    }

    if( hal_device_property_exists( hal_ctx, udi, "volume.policy.desired_mount_point" ) )
        label = hal_device_get_property_string( hal_ctx, udi, "volume.policy.desired_mount_point" );
    if( hal_device_property_exists( hal_ctx, udi, "volume.fstype" ) )
        fstype = hal_device_get_property_string( hal_ctx, udi, "volume.fstype" );
    if( hal_device_property_exists( hal_ctx, udi, "volume.policy.mount_option.sync" ) )
        sync = hal_device_get_property_bool( hal_ctx, udi, "volume.policy.mount_option.sync" );
    if( hal_device_property_exists( hal_ctx, udi, "volume.policy.mount_option.noatime" ) )
        noatime = hal_device_get_property_bool( hal_ctx, udi, "volume.policy.mount_option.noatime" );

    exec_pmount( device, fstype, label, sync, noatime, argc-2, argv+2 );

    return 0;
}

