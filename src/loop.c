/**
 * loop.c -- handling loop devices
 *
 * Author: Vincent Fourmond <fourmond@debian.org>
 *         Copyright 2011 by Vincent Fourmond
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* We unfortunately need regular expressions... */
#include <regex.h>

#include <libintl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include "configuration.h"
#include "config.h"
#include "utils.h"

/* Hmmm, I'll need backquotes here... A fun thing to do safely... */


int loopdev_associate(const char * source, char * target, size_t size)
{
  struct stat before, after;
  

}
