/* 
   This program parses a configuration file and displays results. For
   debugging purposes only.
*/

/* 
 * Copyright (c) 2009 Vincent Fourmond <fourmond@debian.org>
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the
 * license.
 */


#include "policy.h"
#include "utils.h"
#include "conffile.h"
#include <stdio.h>

#define DUMP(x) printf(#x ": %s\n", (cf.x ? "yes" :"no"))
#define DUMPI(x) printf(#x ": %d\n", cf.x)

int main(int argc, char *argv[])
{
  if(argc != 2) {
    fprintf(stderr, "Usage: parse_cf file\n");
    return 1;
  }
  
  conffile_read(argv[1]);
}
