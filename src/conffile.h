/**
 * @file conffile.h parsing of the configuration file
 *
 * @author Vincent Fourmond <fourmond@debian.org>
 *         (c) 2009 by Vincent Fourmond
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#ifndef __config_file_h
#define __config_file_h

/**
 * Information that can be read from a configuration file.
 *
 * For now, all of these fields are booleans; false always means more
 * security.
 */
struct ConfFile {
  /**
     Whether or not the users are allowed to run fsck on the devices
     before they are mounted.
   */
  int allow_fsck;
  
};

/** 
    Initializes the given ConfFile structure to default safe values.
 */
void conffile_init(ConfFile * cf);

/**
   Reads configuration information from the given file into the
   structure.
*/
int conffile_read(const char * file, ConfFile * cf);

#endif /* !defined( __config_file_h) */

