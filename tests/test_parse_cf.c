/*
 * Copyright (c) 2011 Vincent Fourmond <fourmond@debian.org>
 * Copyright (c) 2019 Antonin Décimo <antonin.decimo@gmail.com>
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the
 * license.
 */

#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include "conffile.h"
#include "policy.h"

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


static const char content[] =
  "# test configuration file for parse_cf\n"
  "a_allow = true\n"
  "a_allow_user = daemon ,\\\n"
  "\t      nobody\n"
  "a_deny_user = sys, root\n"
  "a_allow_group = tty\n"
  "\n"
  "# Another configuration item\n"
  "truc_allow_user = root\n"
  "\n"
  "# Testing groups:\n"
  "machin_allow_group = root, audio, cdrom\n"
  "\n"
  "# A string list\n"
  "list =   machin  ,  q,  q,  qq, /bidule\n"
  "\n"
  "\n"
  "# Configuration item not in the list ?\n"
  "bidule = false\n";

static FILE *
prepare(void)
{
    FILE *f = tmpfile();
    int rc;

    enable_debug = 1;

    if(f == NULL) {
        perror("tmpfile");
        exit(EXIT_FAILURE);
    }

    rc = fwrite(content, 1, sizeof(content), f);
    if(rc != sizeof(content)) {
        perror("fwrite");
        exit(EXIT_FAILURE);
    }
    rewind(f);
    return f;
}

int main(void)
{
  FILE * f = prepare();
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

  exit(EXIT_SUCCESS);
}
