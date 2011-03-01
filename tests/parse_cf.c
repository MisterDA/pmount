/* 
 * Copyright (c) 2011 Vincent Fourmond <fourmond@debian.org>
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the
 * license.
 */


#include "policy.h"
#include "utils.h"
#include <stdio.h>
#include "conffile.h"

/* 
   This program checks the configuration file parsing works.

   It reads the parse_cf.conf configuration file.
*/

ci_bool a = {.def = 0};
ci_bool truc = {.def = 0};

cf_spec config[] = {
  {"a", boolean_item, &a},
  {"truc", boolean_item, &truc},
  {NULL}
};


int main()
{
  FILE * f = fopen("parse_cf.conf", "r");
  fprintf(stderr, "An unknown key error must occur "
	  "in the parsing of the file\n");
  cf_read_file(f, config);

  fprintf(stderr, "a values:\n");
  ci_bool_dump(&a, stderr);

  fprintf(stderr, "\ntruc values:\n");
  ci_bool_dump(&truc, stderr);

  fclose(f);
}
