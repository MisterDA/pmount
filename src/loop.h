/**
 * @file loop.h
 *
 * @author Vincent Fourmond <fourmond@debian.org>
 *         Copyright 2011 by Vincent Fourmond
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#ifndef __loop_h
#define __loop_h

/**
   Attempts to associate the given source file to a loop device.

   It checks:

   * that the real user owns the file (too restrictive ?)
   * that read-write access is allowed
   * that the file hasn't been tampered with during the call to losetup.

   It is safe to call this function with source = target

   @todo maybe implement read-only possibilities ?

   Returns 0 on success and -1 on errors.
 */
int loopdev_associate(const char * source, char ** target);


/**
   Dissociates the given loop device

   Returns 0 on success and -1 on errors.
*/
int loopdev_dissociate(const char * dev);

#endif
