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

enum decrypt_status
luks_decrypt( const char* device, char* decrypted, int decrypted_size, 
        const char* password_file )
{
    int status;
    char* label;
    enum decrypt_status result = DECRYPT_NOTENCRYPTED;

    /* check if encrypted */
    status = spawn( SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR, 
            CRYPTSETUP, CRYPTSETUP, "isLuks", device, NULL );
    if( status != 0 ) {
        /* just return device */
        debug( "device is not LUKS encrypted, or cryptsetup with LUKS support is not installed\n" );
        snprintf( decrypted, decrypted_size, "%s", device );
        return result;
    }

    /* generate device label */
    label = strreplace( device, '/', '_' );

    /* open LUKS device */
    if( password_file )
        status = spawn( SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR, 
                CRYPTSETUP, CRYPTSETUP, "luksOpen", "--key-file",
                password_file, device, label, NULL );
    else
        status = spawn( SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR, 
                CRYPTSETUP, CRYPTSETUP, "luksOpen", device, label, NULL );

    if( status == 0 ) {
        /* yes, we have a LUKS device */
        snprintf( decrypted, decrypted_size, "/dev/mapper/%s", label );
        result = DECRYPT_OK;
    } else {
        if( status == 1 ) {
            result = DECRYPT_FAILED;
        } else {
            fprintf( stderr, "Internal error: cryptsetup luksOpen failed" );
            exit( 100 );
        }
    }

    free( label );
    return result;
}

void
luks_release( const char* device )
{
    spawn( SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR, CRYPTSETUP, CRYPTSETUP,
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
