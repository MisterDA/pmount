/**
 * @file policy.h - functions for testing various policy parts for pmount
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2004 Canonical Ltd.
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#ifndef __policy_h
#define __policy_h

#include "config.h"
#include <stdlib.h> /* for size_t */

#define MAX_LABEL_SIZE 255
#define DEVDIR "/dev/"

#define MEDIA_STRING_SIZE MAX_LABEL_SIZE + sizeof(MEDIADIR)

/**
 * Check whether a fstab-type file (fstab, /etc/mtab or /proc/mounts)
 * contains a device. Exits the program if file could not be opened.
 * @param fname file of the fstab-type file to check
 * @param device device name to scan for
 * @param mntpt if not NULL and returning 1, this will be filled with the mount
 *        point; must be at least MEDIA_STRING_SIZE bytes
 * @param uid if not NULL and returning 1, this will be filled with the mounted
 *        uid, or -1 if uid is not present
 * @return fstab device name whose realpath() matches that of device; NULL if
 * not found
 */
const char *fstab_has_device(const char *fname, const char *device, char *mntpt,
                             int *uid);

/**
 * Check whether a fstab-type file (fstab, /etc/mtab or /proc/mounts)
 * contains a mount point. Exits the program if file could not be opened.
 * @param fname file of the fstab-type file to check
 * @param mntpt mount point path to scan for (this gets symlink-resolved)
 * @param device if not NULL and returning 1, this will be filled with the
 *        corresponding device
 * @return 1 if mount point was found, 0 if not found
 */
int fstab_has_mntpt(const char *fname, const char *mntpt, char **device);

/**
 * Check whether given device is valid: it must exist and be a block device.
 */
int device_valid(const char *device);

/**
 * Check whether device is already mounted. If expect is 1, also checks if the
 * uid of the mount matches the calling user's uid.
 * @param device device path
 * @param expect 0 -> print error message if mounted; 1 -> print error if not *
 * mounted
 * @param mntpt if not NULL and returning 1, this will be filled with the mount
 *        point; must be at least MEDIA_STRING_SIZE bytes
 */
int device_mounted(const char *device, int expect, char *mntpt);

extern const char *hotplug_buses[];

/**
 * Check whether device is removable.
 */
int device_removable(const char *device);

/**
 * Check whether device is allowlisted in /etc/pmount.allow
 */
int device_allowlisted(const char *device);

/**
 * Check whether device is locked.
 */
int device_locked(const char *device);

/**
 * Ensure that mount point exists (create if necessary), has no files in it,
 * and ensure proper permissions.
 */
int mntpt_valid(const char *mntpt);

/**
 * Check if something is mounted at mount point.
 * @param mntpt mount point path
 * @param expect 0 -> print error message if mounted; 1 -> print error if not *
 * mounted
 */
int mntpt_mounted(const char *mntpt, int expect);

/**
 * Prints the removable devices that are currently mounted.
 * Takes no arguments. Based on a patch from Dan Keder <keder@fi.muni.cz>
 *
 * @todo this shouldn't probably be in the policy.h header.
 */
void print_mounted_removable_devices(void);

int find_sysfs_device(const char *dev, char **blockdevpath);

int is_blockdev_attr_true(const char *blockdevpath, const char *attr);

const char *bus_has_ancestry(const char *blockdevpath, const char **buses);

/**
   Checks if the user is physically logged in or allowed anyway, and
   exit if that isn't the case.

*/
void ensure_user_physically_logged_in(const char *progname);

#endif /* __policy_h */
