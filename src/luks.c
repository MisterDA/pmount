/**
 * luks.c - cryptsetup/LUKS support for pmount
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2005 Canonical Ltd.
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#define _GNU_SOURCE
#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "luks.h"
#include "policy.h"
#include "utils.h"

/* If CRYPTSETUP_RUID is set, we run cryptsetup with ruid = euid = 0.
   This is due to a recent *feature* in libgcrypt, dropping privileges
   if ruid != euid.

   See http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=551540 for
   more information
 */
#ifdef CRYPTSETUP_RUID
#define CRYPTSETUP_SPAWN_OPTIONS                                               \
    (SPAWN_EROOT | SPAWN_RROOT | SPAWN_NO_STDOUT | SPAWN_NO_STDERR)
#else
#define CRYPTSETUP_SPAWN_OPTIONS                                               \
    (SPAWN_EROOT | SPAWN_NO_STDOUT | SPAWN_NO_STDERR)
#endif

enum decrypt_status
luks_decrypt(const char *device, char **decrypted, const char *password_file,
             int readonly)
{
    int status;
    char *label;
    enum decrypt_status result;
    struct stat st;

    /* check if encrypted */
    status = spawnl(CRYPTSETUP_SPAWN_OPTIONS, CRYPTSETUPPROG, CRYPTSETUPPROG,
                    "isLuks", device, (char *)NULL);
    if(status != 0) {
        /* just return device */
        debug("device is not LUKS encrypted, or cryptsetup with LUKS support "
              "is not installed\n");
        *decrypted = strdup(device);
        if(!*decrypted) {
            perror("strdup(device)");
            exit(E_INTERNAL);
        }
        return DECRYPT_NOTENCRYPTED;
    }

    /* generate device label */
    label = strreplace(device, '/', '_');
    if(asprintf(decrypted, "/dev/mapper/%s", label) == -1) {
        perror("asprintf");
        exit(E_INTERNAL);
    }

    if(!stat(*decrypted, &st))
        return DECRYPT_EXISTS;

    /* open LUKS device */
    if(password_file)
        if(readonly == 1)
            status =
                spawnl(CRYPTSETUP_SPAWN_OPTIONS, CRYPTSETUPPROG, CRYPTSETUPPROG,
                       "luksOpen", "--key-file", password_file, "--readonly",
                       device, label, (char *)NULL);
        else
            status = spawnl(CRYPTSETUP_SPAWN_OPTIONS, CRYPTSETUPPROG,
                            CRYPTSETUPPROG, "luksOpen", "--key-file",
                            password_file, device, label, (char *)NULL);
    else if(readonly == 1)
        status =
            spawnl(CRYPTSETUP_SPAWN_OPTIONS, CRYPTSETUPPROG, CRYPTSETUPPROG,
                   "--readonly", "luksOpen", device, label, (char *)NULL);
    else
        status =
            spawnl(CRYPTSETUP_SPAWN_OPTIONS, CRYPTSETUPPROG, CRYPTSETUPPROG,
                   "luksOpen", device, label, (char *)NULL);

    if(status == 0)
        /* yes, we have a LUKS device */
        result = DECRYPT_OK;
    else if(status == 1)
        result = DECRYPT_FAILED;
    else {
        fprintf(stderr, "Internal error: cryptsetup luksOpen failed\n");
        exit(E_INTERNAL);
    }

    free(label);
    return result;
}

void
luks_release(const char *device, int force)
{
    if(force || luks_has_lockfile(device)) {
        int status = spawnl(CRYPTSETUP_SPAWN_OPTIONS, CRYPTSETUPPROG,
                            CRYPTSETUPPROG, "luksClose", device, (char *)NULL);
        if(status != 0) {
            fprintf(stderr, "Internal error: cryptsetup luksOpen failed\n");
            exit(E_INTERNAL);
        }
        luks_remove_lockfile(device);
    } else
        debug("Not luksClosing '%s' as there is no corresponding lockfile\n",
              device);
}

int
luks_get_mapped_device(const char *device, char **mapped_device)
{
    char *dmlabel = strreplace(device, '/', '_');
    struct stat st;
    if(asprintf(mapped_device, "/dev/mapper/%s", dmlabel) == -1) {
        perror("asprintf");
        return 0;
    }
    free(dmlabel);
    if(stat(*mapped_device, &st) == -1) {
        free(*mapped_device);
        *mapped_device = NULL;
        return 0;
    }
    return 1;
}

#define LUKS_LOCKDIR LOCKDIR "_luks"

static int
luks_lockfile_name(const char *device, char **target)
{
    char *dmlabel = strreplace(device, '/', '_');
    if(asprintf(target, "%s/%s", LUKS_LOCKDIR, dmlabel) == -1) {
        perror("asprintf");
        return 0;
    }
    free(dmlabel);
    return 1;
}

/**
   Creates a 'lockfile' for a given luks device. Returns:
   * 1 on success
   * 0 on error
 */
int
luks_create_lockfile(const char *device)
{
    char *path;
    int f, rc;
    if(assert_dir(LUKS_LOCKDIR, 0))
        return 0; /* Failed for some reason */
    rc = luks_lockfile_name(device, &path);
    if(rc)
        return 0;

    debug("Creating luks lockfile '%s' for device '%s'", path, device);
    get_root();
    f = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    drop_root();
    if(f == -1) {
        fprintf(stderr, "open(%s): %s\n", path, strerror(errno));
        free(path);
        return 0;
    }
    close(f);
    free(path);
    return 1;
}

int
luks_has_lockfile(const char *device)
{
    char *path;
    struct stat st;
    int rc;

    rc = luks_lockfile_name(device, &path);
    if(rc)
        exit(E_INTERNAL);
    debug("Checking luks lockfile '%s' for device '%s'\n", path, device);
    get_root();
    if(!stat(path, &st))
        rc = 1;
    drop_root();
    return rc;
}

void
luks_remove_lockfile(const char *device)
{
    char *path;
    struct stat st;
    int rc;

    rc = luks_lockfile_name(device, &path);
    if(rc)
        exit(E_INTERNAL);
    debug("Removing luks lockfile '%s' for device '%s'\n", path, device);
    get_root();
    if(!stat(path, &st) && !is_dir(path))
        unlink(path);
    drop_root();
}
