/**
 * @file conffile.h parsing of the configuration file
 *
 * @author Vincent Fourmond <fourmond@debian.org>
 *         Copyright 2009-2011 by Vincent Fourmond
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#ifndef __configuration_h
#define __configuration_h

/**
   Returns true if the user is allowed to run fsck
*/
int conffile_allow_fsck();

/**
   Returns true if the user is allowed to use pmount/pumount even
   if not physically logged in
*/
int conffile_allow_not_physically_logged();

/**
   Returns true if the user is allowed to use pmount/pumount to setup
   loopback devices.
*/
int conffile_allow_loop();

/**
   Return the NULL-terminated list of whitelisted loop devices. Can
   return NULL if the list is empty.
*/
char ** conffile_loop_devices();


/**
   Reads configuration information from the given file into the
   structure.

   @return 0 if everything went fine
*/

int conffile_read(const char * file);

/**
   Reads the system configuration file into system_configuration. It
   does not complain on the absence of the system configuration
   file. In this case, the system_configuration file is populated with
   default values (everything potentially dangerous disallowed).

   @return 0 if everything went fine.
 */

int conffile_system_read();

#endif

