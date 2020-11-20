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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int totalTests = 0;
static int testsFailed = 0;

static bool
check_strings_equal(const char *name, const char *str1, const char *str2)
{
    bool rc;
    ++totalTests;
    rc = (!str1 && !str2) || (str1 && str2 && strcmp(str1, str2) == 0);
    fprintf(stderr, "%s (\"%s\", \"%s\"): %s\n", name, str1 ? str1 : "(null)",
            str2 ? str2 : "(null)", rc ? "success" : "failure");
    if(!rc)
        ++testsFailed;
    return rc;
}

int
main(void)
{
    enable_debug = 1;

    /* First checks: fstab_has_device */

    check_strings_equal(
        "check_fstab, simple", "check_fstab/a",
        fstab_has_device("check_fstab/fstab", "check_fstab/a", NULL, NULL));

    check_strings_equal(
        "check_fstab, argument link", "check_fstab/a",
        fstab_has_device("check_fstab/fstab", "check_fstab/b", NULL, NULL));

    check_strings_equal(
        "check_fstab, fstab link", "check_fstab/e",
        fstab_has_device("check_fstab/fstab", "check_fstab/d", NULL, NULL));

    check_strings_equal(
        "check_fstab, fstab double link", "check_fstab/e",
        fstab_has_device("check_fstab/fstab", "check_fstab/c", NULL, NULL));
    fprintf(stderr, "\n%d tests, %d failed\n", totalTests, testsFailed);
    return testsFailed != 0;
}
