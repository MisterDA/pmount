/**
 * luks.c - cryptsetup/LUKS support for pmount
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2005 Canonical Ltd.
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include "luks.h"
#include "utils.h"
#include "policy.h"
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <libintl.h>

enum decrypt_status
luks_decrypt( const char* device, char* decrypted, int decrypted_size, 
        const char* password_file, int readonly )
{
    int status;
    char* label;
    enum decrypt_status result;
    struct stat st;

    /* check if encrypted */
    status = spawnl( SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR, 
            CRYPTSETUP, CRYPTSETUP, "isLuks", device, NULL );
    if( status != 0 ) {
        /* just return device */
        debug( "device is not LUKS encrypted, or cryptsetup with LUKS support is not installed\n" );
        snprintf( decrypted, decrypted_size, "%s", device );
        return DECRYPT_NOTENCRYPTED;
    }

    /* generate device label */
    label = strreplace( device, '/', '_' );
    snprintf( decrypted, decrypted_size, "/dev/mapper/%s", label );

    if( !stat( decrypted, &st) )
        return DECRYPT_EXISTS;

    /* open LUKS device */
    if( password_file )
        if( readonly == 1 )
            status = spawnl( SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR, 
                    CRYPTSETUP, CRYPTSETUP, "luksOpen", "--key-file",
                    password_file, "--readonly", device, label, NULL );
        else
            status = spawnl( SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR, 
                    CRYPTSETUP, CRYPTSETUP, "luksOpen", "--key-file",
                    password_file, device, label, NULL );
    else
        if( readonly == 1 )
            status = spawnl( SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR, 
                    CRYPTSETUP, CRYPTSETUP, "--readonly", "luksOpen",
                    device, label, NULL );
        else
            status = spawnl( SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR, 
                    CRYPTSETUP, CRYPTSETUP, "luksOpen", device, label, NULL );

    if( status == 0 )
        /* yes, we have a LUKS device */
        result = DECRYPT_OK;
    else if( status == 1 )
        result = DECRYPT_FAILED;
    else {
        fprintf( stderr, "Internal error: cryptsetup luksOpen failed" );
        exit( 100 );
    }

    free( label );
    return result;
}

void
luks_release( const char* device )
{
    spawnl( SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR, CRYPTSETUP, CRYPTSETUP,
            "luksClose", device, NULL );
}

int 
luks_get_mapped_device( const char* device, char* mapped_device, 
        size_t mapped_device_size )
{
    char path[PATH_MAX];
    char* dmlabel = strreplace( device, '/', '_' );
    struct stat st;
    snprintf( path, sizeof( path ), "/dev/mapper/%s", dmlabel );
    free( dmlabel );
    if( !stat( path, &st ) ) {
        snprintf( mapped_device, mapped_device_size, "%s", path );
        return 1;
    } else
        return 0;
}

#define LUKS_LOCKDIR LOCKDIR "_luks"

void luks_lockfile_name(const char *device, char * target, size_t t_s)
{
  char* dmlabel = strreplace( device, '/', '_' );
  snprintf(target, t_s, "%s/%s", LUKS_LOCKDIR, dmlabel);
  free(dmlabel);
}

/**
   Creates a 'lockfile' for a given luks device. Returns:
   * 1 on success
   * 0 on error
 */
int luks_create_lockfile(const char * device)
{
  char path[PATH_MAX];
  int f;
  if(assert_dir(LUKS_LOCKDIR, 0))
    return 0;			/* Failed for some reason */
  luks_lockfile_name(device, path, sizeof(path));
  get_root();
  f = creat( path, 0600);
  drop_root();
  if (f < 0) {
    perror( _("luks_create_lockfile(): creat") );
    return 0;
  }
  close(f);
  return 1;
}

