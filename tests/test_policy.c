/* This program checks that policy.c is doing what it should.
   These checks include:

   * fstab_has_device, to check mismatches

*/

/*
 * Copyright (c) 2008 Vincent Fourmond <fourmond@debian.org>
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */


#include "policy.h"
#include "utils.h"
#include <stdio.h>

int testsFailed = 0;
int totalTests = 0;

/* Check that two strings are equal (or not, if _equal_ == 0)*/
void check_strings_equal(const char *name, const char *str1,
			 const char *str2, int equal)
{
  totalTests += 1;
  fprintf(stderr, ".");
  if(enable_debug)
    fprintf(stderr, "1: '%s' 2: '%s'\n", str1?str1:"(null)",
	    str2?str2:"(null)");
  if(!str1 && !str2) {
    if(equal)
      return;
    fprintf(stderr,"%s: both are NULL and should not be identical\n", name);
    testsFailed += 1;
    return;
  }

  if(!str1 || !str2) {
    testsFailed += 1;
    if(! equal)
      return;
    fprintf(stderr,"%s: only one NULL string\n", name);
    return;
  }

  /* Now, neither str1 nor str2 are NULL*/
  if(!strcmp(str1, str2)) {
    /* Both strings are equal */
    if(equal)
      return;

    fprintf(stderr,"%s: both are '%s' and should not be identical\n",
	    name, str1);
    testsFailed += 1;
    return;
  }
  /* Strings are different */
  if(equal) {
    fprintf(stderr,"%s: '%s' is no '%s'\n",
	    name, str1, str2);
    testsFailed += 1;
    return;
  }
  return;
}



int main()
{
  const char * value;
  enable_debug = 0;

 /* First checks: fstab_has_device */

  check_strings_equal("check_fstab, simple", "check_fstab/a",
		      fstab_has_device("check_fstab/fstab", "check_fstab/a",
				       NULL, NULL), 1);

  check_strings_equal("check_fstab, argument link", "check_fstab/a",
		      fstab_has_device("check_fstab/fstab", "check_fstab/b",
				       NULL, NULL), 1);

  check_strings_equal("check_fstab, fstab link", "check_fstab/e",
		      fstab_has_device("check_fstab/fstab", "check_fstab/d",
				       NULL, NULL), 1);

  check_strings_equal("check_fstab, fstab double link", "check_fstab/e",
		      fstab_has_device("check_fstab/fstab", "check_fstab/c",
				       NULL, NULL), 1);
  fprintf(stderr, "\n%d tests, %d failed\n", totalTests, testsFailed);
  return testsFailed != 0;
}
