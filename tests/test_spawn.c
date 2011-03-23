/* 
 * Copyright (c) 2011 Vincent Fourmond <fourmond@debian.org>
 *
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

/**
   This programs checks that the backquotes functions of spawn work
   properly.
 */


#include <stdio.h>
#include <unistd.h>
#include "utils.h"


int main(int argc, char *argv[])
{
  int result;

  result = spawnl(SPAWN_SLURP_STDOUT, "/bin/echo", "echo", 
		  "test string", NULL);
  if(result) {
    fprintf(stderr, "Failed to launch echo\n");
    return 1;
  }
  if(strcmp(slurp_buffer, "test string\n")) {
    fprintf(stderr, "Slurp buffer does not contain expected string, but '%s'\n", 
	    slurp_buffer);
    return 1;
  }
  fprintf(stderr, "Everything went fine, got %s", slurp_buffer);

  /* Now testing that it fails when it should */

  result = spawnl(SPAWN_SLURP_STDOUT, "/", "echo", 
		  "test string", NULL);

  if(! result) {
    fprintf(stderr, "Execution should have failed, but did not");
    return 1;
  }
  return 0;
}
