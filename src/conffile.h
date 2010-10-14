/**
 * @file conffile.h parsing of the configuration file
 *
 * @author Vincent Fourmond <fourmond@debian.org>
 *         (c) 2009 by Vincent Fourmond
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#ifndef __conffile_h
#define __conffile_h

/**
 * Information that can be read from a configuration file.
 *
 * Most of these fields are booleans; false always means more
 * security.
 *
 * @todo This isn't the right way to go. Rather, we should use boolean
 * functions, such as conf_do_i_have_the_right_to_do_this (although
 * maybe in a slightly more compact way ?): this would also handle the
 * complex case when performing an action depends on uid/gid.
 *
 * @todo Eventually, the file system list will be made customizable
 * using the configuration file. Idea: make it possible to overwrite
 * some of the default values ?
 *
 * @todo Make a function that dumps "my permissions" ?
 */
typedef struct {
  /**
     Whether or not the users are allowed to run fsck on the devices
     before they are mounted.
   */
  int allow_fsck;

  /**
     Whether or not the users are allowed mount normal files using
     loopback devices.
   */
  int allow_loop;

  /**
     pmount will probe /dev/loop0, /dev/loop1... until /dev/loopN
     where N is max_loop_device for unused loop devices, and fail if
     it does not find one. This is to avoid DoS by loop device
     exhaustion. Defaults to 0, which means that only /dev/loop0 is
     probed (but of course only if allow_loop is true).
   */
  int max_loop_device;
  
} ConfFile;

/** 
    Initializes the given ConfFile structure to default safe values.
 */
void conffile_init(ConfFile * cf);

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
   The system configuration, as read by conffile_system_read.
 */

extern ConfFile system_configuration;

/**
   Reads the system configuration file into system_configuration. It
   does not complain on the absence of the system configuration
   file. In this case, the system_configuration file is populated with
   default values (everything potentially dangerous disallowed).

   @return 0 if everything went fine.
 */

int conffile_system_read();

#endif /* !defined( __config_file_h) */

