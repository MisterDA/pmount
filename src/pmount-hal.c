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
#include <libhal-storage.h>

#include "policy.h"
#include "fs.h"
#include "utils.h"

/* gettext abbreviation */
#define _(String) gettext(String)

void help() {
    puts( _(
"pmount-hal - execute pmount with additional information from hal\n\n"
"Usage: pmount-hal <device> [pmount options]\n\n"
"This command mounts the device described by the given device or UDI using\n"
"pmount. The file system type, the volume storage policy and the desired label\n"
"will be read out from hal and passed to pmount."));
}

int empty_dir( const char* dirname )
{
    DIR* dir;
    struct dirent* dirent;

    if( !( dir = opendir( dirname ) ) )
        return 1;

    while ( ( dirent = readdir( dir ) ) ) {
        if( strcmp( dirent->d_name, "." ) && 
            strcmp( dirent->d_name, ".created_by_pmount" ) && 
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
        fstab_has_mntpt( "/etc/fstab", mntpt, NULL, 0 ) ||
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
        dbus_bool_t sync, dbus_bool_t noatime, dbus_bool_t exec, const char*
        umask, int addargc, const char* const* addargv ) 
{
    const char** argv = (const char**) calloc( sizeof( const char* ), addargc+13 );
    int argc = 0;
    char freelabel[PATH_MAX];
    int i;

    argv[argc++] = "pmount";

    if( fstype ) {
        argv[argc++] = "-t";
        argv[argc++] = fstype;
    }

    if( sync )
        argv[argc++] = "--sync";
    if( noatime )
        argv[argc++] = "--noatime";
    if( exec )
        argv[argc++] = "--exec";

    if( umask ) {
        argv[argc++] = "--umask";
        argv[argc++] = umask;
    }

    for( i = 0; i < addargc; ++i )
        argv[argc++] = addargv[i];

    argv[argc++] = device;
    if( label ) {
        get_free_label( label, freelabel, sizeof( freelabel ) );
        argv[argc++] = freelabel;
    }
    argv[argc] = NULL;

    /* execute pmount with a new free label until we don't run into a LOCKED
     * condition any more */
    for (;;) {
        int result = spawnv( SPAWN_SEARCHPATH, argv[0], (char *const *) argv );
        if( result == 0 )
            break;
        if( result == 8 ) {
            /* we can into a race condition with a parallel pmount, try again */
            sleep( 1 );
            get_free_label( label, freelabel, sizeof( freelabel ) );
            continue;
        }
        fputs( _("Error: could not execute pmount\n"), stderr );
        exit( 1 );
    }
}

int
main( int argc, const char** argv ) 
{
    const char *devarg;
    LibHalContext *hal_ctx;
    DBusError error;
    DBusConnection *dbus_conn;
    LibHalVolume* volume;
    LibHalDrive* drive;
    dbus_bool_t sync = FALSE, noatime = FALSE, exec = FALSE;
    char *umask = NULL;

    /* initialize locale */
    setlocale( LC_ALL, "" );
    bindtextdomain( "pmount", NULL );
    textdomain( "pmount" );

    if( argc < 2 ) {
        help();
        return 0;
    }

    devarg = argv[1];

    if (getenv ("PMOUNT_DEBUG"))
        enable_debug = 1;

    /* if this is an fstab device, use mount right away */
    if( fstab_has_device( "/etc/fstab", devarg, NULL, NULL ) ) {
	debug( "%s is in /etc/fstab, calling mount\n", devarg );
	return spawnl( SPAWN_SEARCHPATH, "mount", "mount", devarg, NULL );
    }

    /* initialize hal connection */
    dbus_error_init( &error );
    dbus_conn = dbus_bus_get( DBUS_BUS_SYSTEM, &error );
    if( dbus_conn == NULL ) {
        fprintf( stderr, _("Error: could not connect to dbus: %s: %s\n"), error.name, error.message );
        return 1;
    }

    hal_ctx = libhal_ctx_new();
    if( !hal_ctx ) {
        fprintf( stderr, "Error: libhal_ctx_new\n" );
        return 1;
    }

    if( !libhal_ctx_set_dbus_connection( hal_ctx, dbus_conn ) ) {
        fprintf( stderr, "Error: libhal_ctx_set_dbus_connection: %s: %s\n", error.name, error.message );
        return 1;
    }
    if( !libhal_ctx_init( hal_ctx, &error ) ) {
        fprintf(  stderr, "Error: libhal_ctx_init: %s: %s\n", error.name, error.message );
        return 1;
    }

    /* is devarg a proper device node? */
    drive = libhal_drive_from_device_file( hal_ctx, devarg );
    if( drive )
        volume = libhal_volume_from_device_file( hal_ctx, devarg );
    else {
        /* is devarg a volume UDI? */
        volume = libhal_volume_from_udi( hal_ctx, devarg );
        if( volume )
            drive = libhal_drive_from_udi( hal_ctx, libhal_volume_get_storage_device_udi( volume ) );
        else
            /* is devarg a storage UDI? */
            drive = libhal_drive_from_udi( hal_ctx, devarg );
    }

    if( !drive ) {
        fprintf( stderr, _("Error: given UDI is not a mountable volume\n") );
        return 1;
    }

    /* get device */
    const char* device;
    if( volume )
        device = libhal_volume_get_device_file( volume );
    else
        device = libhal_drive_get_device_file( drive );

    if( !device ) {
        fprintf( stderr, "Internal error: UDI has no associated device\n" );
        return 1;
    }

    debug( "drive: %s\nvolume: %s\ndevice: %s\n", libhal_drive_get_udi( drive ),
            volume ? libhal_volume_get_udi( volume ) : "n/a", device );

    /* get label */
    const char* label;
    if( volume ) {
       label = libhal_volume_policy_get_desired_mount_point( drive, volume, NULL );

	if( !label && libhal_device_property_exists( hal_ctx,
		    libhal_volume_get_udi( volume ), "volume.label", &error ) )
	    /* fall back to device label if there is no explicit policy */
	    label = libhal_device_get_property_string( hal_ctx,
		    libhal_volume_get_udi( volume ), "volume.label", &error );
    } else {
       label = libhal_drive_policy_get_desired_mount_point( drive, NULL );

	if( !label && libhal_device_property_exists( hal_ctx,
		    libhal_drive_get_udi( drive ), "volume.label", &error ) )
	    /* fall back to device label if there is no explicit policy */
	    label = libhal_device_get_property_string( hal_ctx,
		    libhal_drive_get_udi( drive ), "volume.label", &error );
    }

    if (!label || !*label)
	label = "usbdisk";

    debug( "label: %s\n", label );

    /* get file system */
    const char* fstype = NULL;
    if( volume )
       fstype = libhal_volume_policy_get_mount_fs( drive, volume, NULL );

    if( !fstype )
        /* fall back to storage device's fstype policy */
       fstype = libhal_drive_policy_get_mount_fs( drive, NULL );

    if( !fstype && volume && libhal_device_property_exists( hal_ctx,
                libhal_volume_get_udi( volume ), "volume.fstype", &error ) )
        /* fall back to plain fstype */
        fstype = libhal_device_get_property_string( hal_ctx,
                libhal_volume_get_udi( volume ), "volume.fstype", &error );

    /* ignore invalid file systems */
    if (fstype && !get_fs_info(fstype)) {
        fstype = NULL;
    }

    debug( "fstype: %s\n", fstype );

    /* mount options */
    const char* options;
    if( volume )
       options = libhal_volume_policy_get_mount_options ( drive, volume, NULL );
    else
       options = libhal_drive_policy_get_mount_options ( drive, NULL );
    debug( "options: %s\n", options );

    const char* s;
    for( s = options; s; s = strchr( s, ',') ) {
        while (*s == ',') ++s; /* skip comma */
        if( !strncmp( s, "exec", 4 ) )
            exec = TRUE;
        else if( !strncmp( s, "noexec", 6 ) )
            exec = FALSE;
        else if( !strncmp( s, "atime", 5 ) )
            noatime = FALSE;
        else if( !strncmp( s, "noatime", 7 ) )
            noatime = TRUE;
        else if( !strncmp( s, "sync", 4 ) )
            sync = TRUE;
        else if( !strncmp( s, "async", 5 ) )
            sync = FALSE;
    }

    /* umask is not covered by the HAL spec */
    const char* computer_udi = "/org/freedesktop/Hal/devices/computer";

    if( volume && libhal_device_property_exists( hal_ctx,
                libhal_volume_get_udi( volume ), "volume.policy.mount_option.umask", &error ) )
        umask = libhal_device_get_property_string( hal_ctx,
                libhal_volume_get_udi( volume ), "volume.policy.mount_option.umask", &error );
    else if( libhal_device_property_exists( hal_ctx, 
                libhal_drive_get_udi( drive ), "storage.policy.mount_option.umask", &error ) )
        umask = libhal_device_get_property_string( hal_ctx,
                libhal_drive_get_udi( drive ), "storage.policy.mount_option.umask", &error );
    else if( libhal_device_property_exists( hal_ctx, computer_udi, "storage.policy.default.mount_option.umask", &error ) )
        umask = libhal_device_get_property_string( hal_ctx, computer_udi, "storage.policy.default.mount_option.umask", &error );

    debug( "umask: %s\n", umask );

    /* shut down hal connection */
    libhal_ctx_shutdown( hal_ctx, &error );
    libhal_ctx_free( hal_ctx );
    dbus_connection_disconnect( dbus_conn );
    dbus_connection_unref( dbus_conn );

    /* go */
    exec_pmount( device, fstype, label, sync, noatime, exec, umask, argc-2, argv+2 );

    return 0;
}

