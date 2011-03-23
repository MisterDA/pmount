/* 
 * Copyright (c) 2011 Vincent Fourmond <fourmond@debian.org>
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the
 * license.
 */


#include "policy.h"
#include <stdio.h>
#include "utils.h"
#include "conffile.h"

/* 
   This program checks the configuration file parsing works.

   It reads the parse_cf.conf configuration file.
*/

ci_bool a = {.def = 0};
ci_bool truc = {.def = 0};
ci_bool machin = {.def = 0};
ci_string_list list;

cf_spec config[] = {
  {"a", boolean_item, &a},
  {"truc", boolean_item, &truc},
  {"machin", boolean_item, &machin},
  {"list", string_list, &list},
  {NULL}
};


int main()
{
  FILE * f = fopen("parse_cf.conf", "r");
  char ** strings;

  fprintf(stderr, "An unknown key error must occur "
	  "in the parsing of the file\n");
  cf_read_file(f, config);

  fprintf(stderr, "a values:\n");
  ci_bool_dump(&a, stderr);

  fprintf(stderr, "\ntruc values:\n");
  ci_bool_dump(&truc, stderr);

  fprintf(stderr, "\nmachin values:\n");
  ci_bool_dump(&machin, stderr);

  fprintf(stderr, "\nlist values:");
  strings = list.strings;
  if(strings)
    while(*strings) {
      fprintf(stderr, " %s", *strings);
      strings++;
    }

  fprintf(stderr, "\n");
  fclose(f);
}
