/**
 * policy.h - functions for testing various policy parts for pmount
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2004 Canonical Ltd.
 *
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#ifndef __policy_h
#define __policy_h

#include <stdlib.h> /* for size_t */

#define MAX_LABEL_SIZE 255
#define MEDIADIR "/media/"
#define DEVDIR "/dev/"
#define LOCKDIR "/var/lock/pmount/"

#define MEDIA_STRING_SIZE MAX_LABEL_SIZE + sizeof( MEDIADIR )

/**
 * Check whether a fstab-type file (fstab, /etc/mtab or /proc/mounts)
 * contains a device. Exits the program if file could not be opened.
 * @param fname file of the fstab-type file to check
 * @param device device name to scan for
 * @param mntpt if not NULL and returning 1, this will be filled with the mount
 *        point; must be at least MEDIA_STRING_SIZE bytes
 * @param uid if not NULL and returning 1, this will be filled with the mounted
 *        uid, or -1 if uid is not present
 * @return fstab device name whose realpath() matches device; NULL if not found
 */
const char* fstab_has_device( const char* fname, const char* device, char* mntpt, int *uid );

/**
 * Check whether a fstab-type file (fstab, /etc/mtab or /proc/mounts)
 * contains a mount point. Exits the program if file could not be opened.
 * @param fname file of the fstab-type file to check
 * @param mntpt mount point path to scan for (this gets symlink-resolved)
 * @param device if not NULL and returning 1, this will be filled with the
 *        corresponding device
 * @param device_size size of the device buffer
 * @return 1 if mount point was found, 0 if not found
 */
int fstab_has_mntpt( const char* fname, const char* mntpt, char* device, size_t device_size );

/**
 * Check whether given device is valid: it must exist and be a block device.
 */
int device_valid( const char* device );

/**
 * Check whether device is already mounted. If expect is 1, also checks if the
 * uid of the mount matches the calling user's uid.
 * @param device device path
 * @param expect 0 -> print error message if mounted; 1 -> print error if not * mounted
 * @param mntpt if not NULL and returning 1, this will be filled with the mount
 *        point; must be at least MEDIA_STRING_SIZE bytes
 */
int device_mounted( const char* device, int expect, char* mntpt );

/**
 * Check whether device is removable.
 */
int device_removable( const char* device );

/**
 * Check whether device is locked.
 */
int device_locked( const char* device );

/**
 * Ensure that mount point exists (create if necessary), has no files in it,
 * and ensure proper permissions.
 */
int mntpt_valid( const char* mntpt );

/**
 * Check if something is mounted at mount point.
 * @param mntpt mount point path
 * @param expect 0 -> print error message if mounted; 1 -> print error if not * mounted
 */
int mntpt_mounted( const char* mntpt, int expect );

/**
 * Construct a lock directory name.
 * @param device lock directory is created for this device
 * @param name string buffer for the created lock directory
 * @param name_size size of buffer name
 */
void
make_lockdir_name( const char* device, char* name, size_t name_size );

#endif /* __policy_h */

