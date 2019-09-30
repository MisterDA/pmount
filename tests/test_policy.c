/* This program checks that policy.c is doing what it should.
   These checks include:

   * fstab_has_device, to check mismatches
*/

/*
 * Copyright (c) 2008 Vincent Fourmond <fourmond@debian.org>
 * Copyright (c) 2019 Antonin Décimo <antonin.decimo@gmail.com>
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "policy.h"
#include "utils.h"

static char template[] = "pmount-XXXXXX";

static void prepare(void)
{
    const char content[] =
        "check_fstab/a /foo btrfs "" 0 0\n"
        "check_fstab/e /foo btrfs "" 0 0\n";
    char *dir;
    FILE *fstab;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int rc;

    enable_debug = 1;

    dir = mkdtemp(template);
    if(dir == NULL) {
        perror("mkdtemp");
        exit(EXIT_FAILURE);
    }

    rc = chdir(dir);
    if(rc == -1) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    rc = mkdir("check_fstab", S_IRWXU);
    if(rc == -1) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }

    rc = chdir("check_fstab");
    if(rc == -1) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    fstab = fopen("fstab", "w+b");
    if(fstab == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    rc = fwrite(content, 1, sizeof(content), fstab);
    if(rc != sizeof(content)) {
        perror("fwrite");
        exit(EXIT_FAILURE);
    }

    rc = fclose(fstab);
    if(rc == EOF) {
        perror("fclose");
        exit(EXIT_FAILURE);
    }

    rc = open("a", O_RDONLY | O_CREAT, mode);
    if(rc == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    rc = close(rc);
    if(rc == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    rc = open("e", O_RDONLY | O_CREAT, mode);
    if(rc == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    rc = close(rc);
    if(rc == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    rc = symlink("a", "b");
    if(rc == -1) {
        perror("symlink");
        exit(EXIT_FAILURE);
    }

    rc = symlink("e", "d");
    if(rc == -1) {
        perror("symlink");
        exit(EXIT_FAILURE);
    }

    rc = symlink("d", "c");
    if(rc == -1) {
        perror("symlink");
        exit(EXIT_FAILURE);
    }

    rc = chdir("..");
    if(rc == -1) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    puts(dir);
}

static void teardown(void)
{
    int rc;
    rc = chdir("..");
    if(rc == -1) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }
    // FIXME: the file tree could be removed
}

/* Check that two strings are equal (or not, if _equal_ == false)*/
static bool check_strings_equal(const char *name, const char *str1,
                                const char *str2, bool equal)
{
    fprintf(stderr, "%s: 1: '%s' 2: '%s'\n", name,
            str1?str1:"(null)", str2?str2:"(null)");
    if(str1 == str2)
        return equal;
    if(!str1 || !str2)
        return !equal;

    return strcmp(str1, str2) == 0 ? equal : !equal;
}

int main(void)
{
    bool ok;
    prepare();

    /* First checks: fstab_has_device */

    ok = check_strings_equal("check_fstab, simple", "check_fstab/a",
                             fstab_has_device("check_fstab/fstab",
                                              "check_fstab/a", NULL, NULL),
                             true);
    if(!ok) {
        fputs("Failing test #1.", stderr);
        teardown();
        exit(EXIT_FAILURE);
    }

    ok = check_strings_equal("check_fstab, argument link", "check_fstab/a",
                             fstab_has_device("check_fstab/fstab",
                                              "check_fstab/b", NULL, NULL),
                             true);
    if(!ok) {
        fputs("Failing test #2.", stderr);
        teardown();
        exit(EXIT_FAILURE);
    }

    ok = check_strings_equal("check_fstab, fstab link", "check_fstab/e",
                             fstab_has_device("check_fstab/fstab",
                                              "check_fstab/d", NULL, NULL),
                             true);
    if(!ok) {
        fputs("Failing test #3.", stderr);
        teardown();
        exit(EXIT_FAILURE);
    }

    ok = check_strings_equal("check_fstab, fstab double link", "check_fstab/e",
                             fstab_has_device("check_fstab/fstab",
                                              "check_fstab/c", NULL, NULL),
                             true);
    if(!ok) {
        fputs("Failing test #4.", stderr);
        teardown();
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
