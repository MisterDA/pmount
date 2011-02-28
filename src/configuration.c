/**
 * conffile.c -- parsing of the configuration file
 *
 * Author: Vincent Fourmond <fourmond@debian.org>
 *         (c) 2009, 2011 by Vincent Fourmond
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include <stdio.h>
#include <stdio.h>
#include <string.h>

#include <libintl.h>
#include <sys/stat.h>

#include "conffile.h"
#include "configuration.h"
#include "config.h"
#include "utils.h"


/**********************************************************************/
/* Configuration items  */

/**
   Whether or not the user is allowed to run fsck or not.
*/

static ci_bool conf_allow_fsck = {
  .def = 0
};

int conffile_allow_fsck()
{
  return ci_bool_allowed(&conf_allow_fsck);
}

cf_spec config[] = {
  {"fsck", boolean_item, &conf_allow_fsck},
  {NULL}
};




int conffile_read(const char * file)
{
  FILE * f;
  int ret;
  f = fopen(file,"r");
  if(! f) {
    perror(_("Failed to open configuration file"));
    return -2;
  }
  ret = cf_read_file(f, config);
  fclose(f);

  return ret;
}

int conffile_system_read()
{
  struct stat st;
  /* If the system configuration file does not exist, we don't
     complain... */
  if( stat( SYSTEM_CONFFILE, &st) )
    return 0;
  return conffile_read(SYSTEM_CONFFILE);
}
