/**
 * @file conffile.h parsing of the configuration file
 *
 * @author Vincent Fourmond <fourmond@debian.org>
 *         (c) 2009 by Vincent Fourmond
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
   if not phyiscally logged in
*/
int conffile_allow_not_physically_logged();

/**
   Returns true if the user is allowed to use pmount/pumount to setup
   loopback devices.
*/
int conffile_allow_loop();


/**
   Reads configuration information from the given file into the
   structure.

   @todo This function should be able to handle more subtle effect,
   such as allowing only specific users/groups, or forbidding given
   users. Maybe this is not an emergency for now.

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

