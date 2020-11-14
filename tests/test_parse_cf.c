/*
 * Copyright (c) 2011 Vincent Fourmond <fourmond@debian.org>
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the
 * license.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "policy.h"
#include "utils.h"
#include "conffile.h"

/*
   This program checks the configuration file parsing works.
   It reads the parse_cf.conf configuration file.
*/

static ci_bool a = {.def = 0};
static ci_bool truc = {.def = 0};
static ci_bool machin = {.def = 0};
static ci_string_list list;

static cf_spec config[] = {
  {.base = "a", .type = boolean_item, .boolean_item = &a},
  {.base = "truc", .type = boolean_item, .boolean_item = &truc},
  {.base = "machin", .type = boolean_item, .boolean_item = &machin},
  {.base = "list", .type = string_list, .string_list = &list},
  {.base = NULL}
};


int main(int argc, const char *argv[])
{
  FILE * f;
  char ** strings;

  if(argc != 2) {
    fprintf(stderr, "Usage: %s conffile\n", argv[0]);
    return EXIT_FAILURE;
  }

  f = fopen(argv[1], "r");
  if(!f) {
    fprintf(stderr, "fopen(%s): %s\n", argv[1], strerror(errno));
    return EXIT_FAILURE;
  }

  fprintf(stderr,
          "An unknown key error must occur in the parsing of the file\n");
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
  return EXIT_SUCCESS;
}
