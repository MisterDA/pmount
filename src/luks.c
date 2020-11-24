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

/**
   Creates a 'lockfile' for a given luks device. Returns:
   * 1 on success
   * 0 on error
 */
int
luks_create_lockfile(const char *device)
{
    int lockdir_fd, fd;
    char *device_name;
    int rc = 0;

    lockdir_fd = assert_dir(LUKS_LOCKDIR, 0);
    if(lockdir_fd < 0)
        return rc;
    device_name = make_lock_name(device);
    if(device_name == NULL)
        goto lockdir_fd;

    debug("Creating luks lockfile '%s/%s' for device '%s'", LUKS_LOCKDIR,
          device_name, device);
    get_root();
    fd = openat(lockdir_fd, device_name, O_WRONLY | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR);
    drop_root();
    if(fd == -1) {
        fprintf(stderr, "open(%s/%s): %s\n", LUKS_LOCKDIR, device_name,
                strerror(errno));
        goto device_name;
    }
    rc = 1;
    close(fd);
device_name:
    free(device_name);
lockdir_fd:
    close(lockdir_fd);
    return rc;
}

int
luks_has_lockfile(const char *device)
{
    char *path;
    struct stat st;
    int rc = 0;

    path = make_lock_path(LUKS_LOCKDIR, device);
    if(path == NULL)
        return rc;
    debug("Checking luks lockfile '%s' for device '%s'\n", path, device);
    get_root();
    if(!stat(path, &st))
        rc = 1;
    drop_root();
    free(path);
    return rc;
}

void
luks_remove_lockfile(const char *device)
{
    char *path;
    int rc, saved_errno;

    path = make_lock_path(LUKS_LOCKDIR, device);
    if(path == NULL)
        return;

    debug("Removing luks lockfile '%s' for device '%s'\n", path, device);
    get_root();
    rc = unlink(path);
    if(rc < 0)
        saved_errno = errno;
    drop_root();
    if(rc < 0)
        fprintf(stderr, "unlink(%s): %s\n", path, strerror(saved_errno));
    free(path);
}
