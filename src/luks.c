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
#include "config.h"
#include "utils.h"
#include "policy.h"
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <libintl.h>


/* If CRYPTSETUP_RUID is set, we run cryptsetup with ruid = euid = 0.
   This is due to a recent *feature* in libgcrypt, dropping privileges
   if ruid != euid.

   See http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=551540 for
   more information
 */
#ifdef CRYPTSETUP_RUID
#define CRYPTSETUP_SPAWN_OPTIONS (SPAWN_EROOT|SPAWN_RROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR)
#else
#define CRYPTSETUP_SPAWN_OPTIONS (SPAWN_EROOT|SPAWN_NO_STDOUT|SPAWN_NO_STDERR)
#endif

enum decrypt_status
luks_decrypt( const char* device, char* decrypted, int decrypted_size, 
        const char* password_file, int readonly )
{
    int status;
    char* label;
    enum decrypt_status result;
    struct stat st;

    /* check if encrypted */
    status = spawnl( CRYPTSETUP_SPAWN_OPTIONS, 
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
            status = spawnl( CRYPTSETUP_SPAWN_OPTIONS, 
                    CRYPTSETUP, CRYPTSETUP, "luksOpen", "--key-file",
                    password_file, "--readonly", device, label, NULL );
        else
            status = spawnl( CRYPTSETUP_SPAWN_OPTIONS, 
                    CRYPTSETUP, CRYPTSETUP, "luksOpen", "--key-file",
                    password_file, device, label, NULL );
    else
        if( readonly == 1 )
            status = spawnl( CRYPTSETUP_SPAWN_OPTIONS, 
                    CRYPTSETUP, CRYPTSETUP, "--readonly", "luksOpen",
                    device, label, NULL );
        else
            status = spawnl( CRYPTSETUP_SPAWN_OPTIONS, 
                    CRYPTSETUP, CRYPTSETUP, "luksOpen", device, label, NULL );

    if( status == 0 )
        /* yes, we have a LUKS device */
        result = DECRYPT_OK;
    else if( status == 1 )
        result = DECRYPT_FAILED;
    else {
        fprintf( stderr, "Internal error: cryptsetup luksOpen failed\n" );
        exit( 100 );
    }

    free( label );
    return result;
}

void
luks_release( const char* device, int force )
{
  if(force || luks_has_lockfile(device)) {
    spawnl( CRYPTSETUP_SPAWN_OPTIONS, 
	    CRYPTSETUP, CRYPTSETUP, "luksClose", device, NULL );
    luks_remove_lockfile(device);
  }
  else
    debug("Not luksClosing '%s' as there is no corresponding lockfile\n",
	  device);
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

  debug("Creating luks lockfile '%s' for device '%s'",
	path, device);
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

int luks_has_lockfile(const char * device)
{
  char path[PATH_MAX];
  struct stat st;
  int ret = 0;

  luks_lockfile_name(device, path, sizeof(path));
  debug("Checking luks lockfile '%s' for device '%s'\n",
	path, device);
  get_root();
  if(!stat(path,&st)) 
    ret = 1;
  drop_root();
  return ret;
}

void luks_remove_lockfile(const char * device)
{
  char path[PATH_MAX];
  struct stat st;

  luks_lockfile_name(device, path, sizeof(path));
  debug("Removing luks lockfile '%s' for device '%s'\n",
	path, device);
  get_root();
  if(!stat(path,&st) && !is_dir(path)) 
    unlink(path);
  drop_root();
}
