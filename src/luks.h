/**
 * luks.h - cryptsetup/LUKS support for pmount
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2005 Canonical Ltd.
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#ifndef __luks_h
#define __luks_h

#include <stdlib.h>

enum decrypt_status {DECRYPT_OK, DECRYPT_NOTENCRYPTED, DECRYPT_FAILED,
    DECRYPT_EXISTS};

/**
 * Check whether the given device is encrypted using dmcrypt with LUKS
 * metadata; if so, call cryptsetup to setup the device.
 * @param device raw device name
 * @param decrypted buffer for decrypted device; if device is unencrypted,
 *        this will be set to device
 * @param decrypted_size size of the "decrypted" buffer
 * @param password_file file to read the password from (NULL means prompt)
 * @param readonly 1 if device is read-only
 */
enum decrypt_status luks_decrypt( const char* device, char* decrypted, 
        int decrypted_size, const char* password_file, int readonly );

/**
 * Check whether device is mapped through cryptsetup, and release it if
 * one of the given conditions are met:
 * - if force is true
 * - if the corresponding lockfile exists.
 */
void luks_release( const char* device, int force );

/**
 * Check whether the given real device has been mapped to a dmcrypt device. If
 * so, return the mapped device in mapped_device and return 1, otherwise return
 * 0.
 */
int luks_get_mapped_device( const char* device, char* mapped_device, 
        size_t mapped_device_size );

/**
 * Creates a 'lockfile' to remember that the given luks device was
 * luksOpened by pmount. Returns 1 on success, 0 on error.
 */
int luks_create_lockfile(const char * device);

/**
 * Checks the presence of the luks 'lockfile' for a given device.
 * Returns 1 if the lockfile is present.
 */
int luks_has_lockfile(const char * device);

/**
 * Removes the luks 'lockfile' corresponding to the given device.
 * Never fails. But might not really remove it if it is a directory.
 */
void luks_remove_lockfile(const char * device);

#endif /* !defined( __luks_h) */
