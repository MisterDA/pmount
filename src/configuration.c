/**
 * conffile.c -- parsing of the configuration file
 *
 * Author: Vincent Fourmond <fourmond@debian.org>
 *         (c) 2009, 2011 by Vincent Fourmond
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include "config.h"
#include <stdio.h>
#include <string.h>

#include <libintl.h>
#include <sys/stat.h>

#include "conffile.h"
#include "configuration.h"
#include "utils.h"

/**********************************************************************/
/* Configuration items  */

/**
   Whether or not the user is allowed to run fsck or not.
*/

static ci_bool conf_allow_fsck = { .def = 0 };

int
conffile_allow_fsck(void)
{
    return ci_bool_allowed(&conf_allow_fsck);
}

static ci_bool conf_allow_not_physically_logged = { .def = 0 };

int
conffile_allow_not_physically_logged(void)
{
    return ci_bool_allowed(&conf_allow_not_physically_logged);
}

static ci_bool conf_allow_loop = { .def = 0 };

int
conffile_allow_loop(void)
{
    return ci_bool_allowed(&conf_allow_loop);
}

static ci_string_list conf_loop_devices = { .strings = NULL };

char **
conffile_loop_devices(void)
{
    return conf_loop_devices.strings;
}

static cf_spec config[] = {
    { .base = "fsck", .type = boolean_item, .boolean_item = &conf_allow_fsck },
    { .base = "not_physically_logged",
      .type = boolean_item,
      .boolean_item = &conf_allow_not_physically_logged },
    { .base = "loop", .type = boolean_item, .boolean_item = &conf_allow_loop },
    { .base = "loop_devices",
      .type = string_list,
      .string_list = &conf_loop_devices },
    { .base = NULL }
};

int
conffile_read(const char *file)
{
    FILE *f;
    int ret;
    f = fopen(file, "r");
    if(!f) {
        perror(_("Failed to open configuration file"));
        return -2;
    }
    ret = cf_read_file(f, config);
    fclose(f);

    return ret;
}

int
conffile_system_read(void)
{
    struct stat st;
    /* If the system configuration file does not exist, we don't
       complain... */
    if(stat(SYSTEM_CONFFILE, &st))
        return 0;
    return conffile_read(SYSTEM_CONFFILE);
}
