/**
 * conffile.c -- parsing of the configuration file
 *
 * Author: Vincent Fourmond <fourmond@debian.org>
 *         (c) 2009 by Vincent Fourmond
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include "config-file.h"
#include <string.h>

void conffile_init(ConfFile * cf)
{
  memset(cf, 0, sizeof(ConfFile));
}
