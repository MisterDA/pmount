/**
 * policy.c - functions for testing various policy parts for pmount
 *
 * Authors: Martin Pitt <martin.pitt@canonical.com>,
 *          Vincent Fourmond <fourmond@debian.org
 * (c) 2004 Canonical Ltd,
 *     2007, 2008, 2009 by Vincent Fourmond
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#define _GNU_SOURCE
#include "policy.h"
#include "utils.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <libintl.h>
#include <limits.h>
#include <mntent.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

/* For globs in /etc/pmount.allow */
#include <fnmatch.h>

/* For passwd and utmp parsing */
#include <pwd.h>
#include <sys/types.h>
#include <utmpx.h>

#include "configuration.h"

/*************************************************************************
 *
 * Sysfs query functions for determining if a device is removable
 *
 *************************************************************************/

/**
   The directories to search for to find the block subsystem. Null-terminated.
 */
static const char *block_subsystem_directories[] = {
    "/sys/subsystem/block",
    "/sys/class/block",
    "/sys/block",
    NULL,
};

/**
 * Find sysfs node that matches the major and minor device number of
 * the given device. Exit the process immediately on errors.
 * @param dev device node to search for (e.g., /dev/sda1)
 * @param blockdevpath if not NULL, the corresponding /<sysfs>/<drive>/
 *        path is written into this buffer; this can be used to query
 *        additional attributes
 * @return 0 if device was found and -1 if it was not.
 */
int
find_sysfs_device(const char *dev, char **blockdevpath)
{
    unsigned int devmajor, devminor;
    const char **block;
    char *blockdirname;
    DIR *devdir;
    struct dirent *devdirent;
    struct stat devstat;
    int rc = 0; /* Failing by default. */

    /* determine major and minor of dev */
    if(stat(dev, &devstat)) {
        perror(_("Error: could not get status of device"));
        exit(E_INTERNAL);
    }
    devmajor = major(devstat.st_rdev);
    devminor = minor(devstat.st_rdev);

    debug("find_sysfs_device: looking for sysfs directory for device %u:%u\n",
          devmajor, devminor);

    /* We first need to find one of /sys/subsystem/block,
     * /sys/class/block or /sys/block. And then, we look for the right
     * device number. */
    for(block = block_subsystem_directories; *block; block++) {
        if(!stat(*block, &devstat)) {
            debug("found block subsystem at: %s\n", *block);
            break;
        }
    }
    if(!*block) {
        perror(_("Error: could find the block subsystem directory"));
        exit(E_INTERNAL);
    }
    if(asprintf(&blockdirname, "%s/", *block) == -1) {
        perror("asprintf");
        exit(E_INTERNAL);
    }
    devdir = opendir(blockdirname);
    if(!devdir) {
        perror(_("Error: could not open <sysfs dir>/block/"));
        exit(E_INTERNAL);
    }

    /* open each subdirectory and see whether major device matches */
    while((devdirent = readdir(devdir)) != NULL) {
        unsigned char sysmajor, sysminor;
        char *devdirname, *devfilename;

        if(asprintf(&devdirname, "%s%s", blockdirname, devdirent->d_name) ==
               -1 ||
           asprintf(&devfilename, "%s/dev", devdirname) == -1) {
            perror("asprintf");
            exit(E_INTERNAL);
        }

        /* read the block device major:minor */
        if(read_number_colon_number(devfilename, &sysmajor, &sysminor) == -1) {
            free(devdirname);
            free(devfilename);
            continue;
        }
        free(devfilename);

        debug("find_sysfs_device: checking whether %s is on %s (%u:%u)\n", dev,
              devdirname, (unsigned)sysmajor, (unsigned)sysminor);

        if(sysmajor == devmajor) {
            debug("find_sysfs_device: major device numbers match\n");

            /* if dev is a partition, check that there is a subdir
             * that matches the partition */
            if(sysminor != devminor) {
                DIR *partdir;
                struct dirent *partdirent;
                bool found_part = false;

                debug("find_sysfs_device: minor device numbers do not match, "
                      "checking partitions...\n");

                partdir = opendir(devdirname);
                if(!partdir) {
                    perror(
                        _("Error: could not open <sysfs dir>/block/<device>/"));
                    exit(E_INTERNAL);
                }
                while((partdirent = readdir(partdir)) != NULL) {
                    if(partdirent->d_type != DT_DIR)
                        continue;

                    if(asprintf(&devfilename, "%s/%s/%s", devdirname,
                                partdirent->d_name, "dev") == -1) {
                        perror("asprintf");
                        exit(E_INTERNAL);
                    }

                    /* read the block device major:minor */
                    if(read_number_colon_number(devfilename, &sysmajor,
                                                &sysminor) == -1) {
                        free(devfilename);
                        continue;
                    }

                    debug("find_sysfs_device: checking whether device %s "
                          "matches partition %u:%u\n",
                          dev, (unsigned)sysmajor, (unsigned)sysminor);

                    if(sysmajor == devmajor && sysminor == devminor) {
                        debug("find_sysfs_device: -> partition matches, "
                              "belongs to block device %s\n",
                              devdirname);
                        found_part = true;
                        free(devfilename);
                        break;
                    }
                    free(devfilename);
                }
                closedir(partdir);

                if(!found_part) {
                    /* dev is a partition, but it does not belong to the
                     * currently examined device; skip to next device */
                    continue;
                }
            } else {
                debug("find_sysfs_device: minor device numbers also match, %s "
                      "is a raw device\n",
                      dev);
            }

            if(blockdevpath) {
                *blockdevpath = devdirname;
            } else {
                debug("WARNING: find_sysfs_device is called without "
                      "blockdevpath argument\n");
                free(devdirname);
            }
            rc = 1; /* We found it ! */
            break;
        }
    }

    closedir(devdir);
    free(blockdirname);
    return rc;
}

/**
   Return whether attribute attr in blockdevpath exists and has value '1'.

   Or, in other words, if blockdevpath/attr exists and contains a '1' as
   its first character.
 */
int
is_blockdev_attr_true(const char *blockdevpath, const char *attr)
{
    char *path;
    FILE *f;
    int result;
    char value;

    if(asprintf(&path, "%s/%s", blockdevpath, attr) == -1) {
        perror("asprintf");
        exit(E_INTERNAL);
    }

    f = fopen(path, "r");
    if(!f) {
        debug("is_blockdev_attr_true: could not open %s\n", path);
        free(path);
        return 0;
    }

    result = fread(&value, 1, 1, f);
    fclose(f);

    if(result != 1) {
        debug("is_blockdev_attr_true: could not read %s\n", path);
        free(path);
        return 0;
    }

    debug("is_blockdev_attr_true: value of %s == %c\n", path, value);
    free(path);
    return value == '1';
}

/*************************************************************************/
/* Bus-related functions

   WARNING. Quoting Documentation/sysfs-rules.txt:

   - devices are only "devices"
   There is no such thing like class-, bus-, physical devices,
   interfaces, and such that you can rely on in userspace. Everything is
   just simply a "device". Class-, bus-, physical, ... types are just
   kernel implementation details which should not be expected by
   applications that look for devices in sysfs.

   Therefore, the notion of 'bus' is at best not reliable. But I still
   keep the information, as it could help in corner cases.
*/

/**
   Tries to find the 'bus' of the given *device* (ie, under
   /sys/devices), and stores is into the bus string.

   Note that this function is in no way guaranteed to work, as the bus
   attribute is "fragile". But I'm not aware of anything better for
   now.

   This function was rewritten from scratch by
   Heinz-Ado Arnolds <arnolds@mpa-garching.mpg.de>, with a much better
   knowledge than me about the newer sysfs architecture.

   Many thanks !
 */

static const char *
get_device_bus(const char *devicepath, const char **buses)
{
    const char *res = NULL;
    const char **i;

    for(i = buses; *i; i++) {
        struct dirent *busdirent;
        DIR *busdir;
        char *path;

        if(asprintf(&path, "/sys/bus/%s/devices", *i) == -1) {
            debug("asprintf: %s\n", strerror(errno));
            continue;
        }
        if(!(busdir = opendir(path))) {
            debug("opendir(%s): %s\n", path, strerror(errno));
            free(path);
            continue;
        }

        while(!res && (busdirent = readdir(busdir))) {
            char *devfilename, *link;
            if(asprintf(&devfilename, "%s/%s", path, busdirent->d_name) == -1) {
                debug("asprintf: %s\n", strerror(errno));
                continue;
            }
            if(!(link = realpath(devfilename, NULL))) {
                debug("realpath(%s): %s\n", devfilename, strerror(errno));
                free(devfilename);
                continue;
            }
            if(strcmp(devicepath, link) == 0)
                res = *i;
            free(devfilename);
            free(link);
        }

        closedir(busdir);
        free(path);
        if(res)
            return res;
    }

    return NULL;
}

/**
 * Check whether a bus occurs anywhere in the ancestry of a device.
 * @param blockdevpath is a device as returned by
 * @param buses NULL-terminated array of bus names to scan for
 * @return the name of the bus found, or NULL
 */
const char *
bus_has_ancestry(const char *blockdevpath, const char **buses)
{
    char *path, *full_device, *tmp;
    struct stat sb;
    int rc;

    /* The sysfs structure has changed: in former times /sys/block/<dev>
     * was a directory and /sys/block/<dev>/device a link to the real
     * device dir. Now (linux-2.6.27.9) /sys/block/<dev> is a link to
     * the real device dir. */
    rc = lstat(blockdevpath, &sb);
    if(rc == -1) {
        debug("lstat(%s): %s\n", blockdevpath, strerror(errno));
        return NULL;
    }
    tmp = S_ISLNK(sb.st_mode) ? "" : "/device";
    if(asprintf(&path, "%s%s", blockdevpath, tmp) == -1) {
        debug("asprintf: %s\n", strerror(errno));
        return NULL;
    }
    if(!(full_device = realpath(path, NULL))) {
        debug("realpath(%s): %s\n", path, strerror(errno));
        free(path);
        return NULL;
    }
    free(path);

    /* We now have a full path to the device */

    /* We loop on full_device until we are on the root directory */
    while(full_device[0]) {
        const char *bus = get_device_bus(full_device, buses);
        if(bus) {
            debug("Found bus %s for device %s\n", bus, full_device);
            free(full_device);
            return bus;
        }
        tmp = strrchr(full_device, '/');
        if(!tmp)
            break;
        *tmp = 0;
    }
    free(full_device);
    return NULL;
}

/*************************************************************************
 *
 * Policy functions
 *
 *************************************************************************/

int
device_valid(const char *device)
{
    struct stat st;

    if(stat(device, &st)) {
        fprintf(stderr, _("Error: device %s does not exist\n"), device);
        return 0;
    }

    if(!S_ISBLK(st.st_mode)) {
        fprintf(stderr, _("Error: %s is not a block device\n"), device);
        return 0;
    }

    return 1;
}

const char *
fstab_has_device(const char *fname, const char *device, char *mntpt, int *uid)
{
    FILE *f;
    struct mntent *entry;
    char *pathbuf_arg;
    static char fstab_device[PATH_MAX];
    const char *realdev_arg;

    debug("Checking for device '%s' in '%s'\n", device, fname);

    if(!(f = fopen(fname, "r"))) {
        perror(_("Error: could not open fstab-type file"));
        exit(E_INTERNAL);
    }

    if((pathbuf_arg = realpath(device, NULL))) {
        realdev_arg = pathbuf_arg;
    } else {
        debug("realpath(%s): %s\n", device, strerror(errno));
        realdev_arg = device;
    }

    while((entry = getmntent(f)) != NULL) {
        char *pathbuf, *realdev;
        snprintf(fstab_device, sizeof(fstab_device), "%s", entry->mnt_fsname);

        if((pathbuf = realpath(fstab_device, NULL)))
            realdev = pathbuf;
        else
            realdev = fstab_device;

        if(!strcmp(realdev, realdev_arg)) {
            if(mntpt) {
                snprintf(mntpt, MEDIA_STRING_SIZE - 1, "%s", entry->mnt_dir);
            }
            if(uid) {
                char *uidopt = hasmntopt(entry, "uid");
                if(uidopt)
                    uidopt = strchr(uidopt, '=');
                if(uidopt) {
                    ++uidopt; /* skip the '=' */
                    /* FIXME: this probably needs more checking */
                    *uid = atoi(uidopt);
                } else
                    *uid = -1;
            }

            endmntent(f);
            free(pathbuf);
            free(pathbuf_arg);
            debug(" -> found as '%s'\n", fstab_device);
            return fstab_device;
        }
        free(pathbuf);
    }

    /* just for safety */
    if(mntpt)
        *mntpt = 0;

    endmntent(f);
    free(pathbuf_arg);
    debug(" -> not found\n");
    return NULL;
}

int
fstab_has_mntpt(const char *fname, const char *mntpt, char **device)
{
    FILE *f;
    struct mntent *entry;
    char *realmntptbuf;
    const char *realmntpt, *fstabmntpt;
    int rc = 0;
    *device = NULL;

    /* resolve symlinks, if possible */
    if((realmntptbuf = realpath(mntpt, NULL)))
        realmntpt = realmntptbuf;
    else
        realmntpt = mntpt;

    if(!(f = fopen(fname, "r"))) {
        perror(_("Error: could not open fstab-type file"));
        exit(E_INTERNAL);
    }

    while((entry = getmntent(f)) != NULL) {
        char *fstabmntptbuf;
        /* resolve symlinks, if possible */
        if((fstabmntptbuf = realpath(entry->mnt_dir, NULL)))
            fstabmntpt = fstabmntptbuf;
        else
            fstabmntpt = entry->mnt_dir;

        if(!strcmp(fstabmntpt, realmntpt)) {
            if(device) {
                *device = strdup(entry->mnt_fsname);
                if(!*device) {
                    perror("strdup");
                    exit(E_INTERNAL);
                }
            }
            rc = 1;
            free(fstabmntptbuf);
            goto done;
        }
        free(fstabmntptbuf);
    }

done:
    free(realmntptbuf);
    endmntent(f);
    return rc;
}

int
device_mounted(const char *device, int expect, char *mntpt)
{
    char mp[MEDIA_STRING_SIZE];
    int uid;
    int mounted = fstab_has_device("/etc/mtab", device, mp, &uid) ||
                  fstab_has_device("/proc/mounts", device, mp, &uid);
    if(mounted && !expect)
        fprintf(stderr, _("Error: device %s is already mounted to %s\n"),
                device, mp);
    else if(!mounted && expect)
        fprintf(stderr, _("Error: device %s is not mounted\n"), device);
    if(mounted && expect && uid >= 0 && (uid_t)uid != getuid() &&
       getuid() > 0) {
        fprintf(stderr, _("Error: device %s was not mounted by you\n"), device);
        return 0;
    }

    if(mntpt)
        strncpy(mntpt, mp, MEDIA_STRING_SIZE);

    return mounted;
}

const char *hotplug_buses[] = {
    "usb", "ieee1394", "mmc", "pcmcia", "firewire", NULL,
};

/* The silent version of the device_removable function. */
static int
device_removable_silent(const char *device)
{
    int removable;
    char *blockdevpath;

    if(!find_sysfs_device(device, &blockdevpath)) {
        debug("device_removable: could not find a sysfs device for %s\n",
              device);
        return 0;
    }

    debug("device_removable: corresponding block device for %s is %s\n", device,
          blockdevpath);

    /* check whether device has "removable" attribute with value '1' */
    removable = is_blockdev_attr_true(blockdevpath, "removable");

    /*
       If not, fall back to bus scanning (regard USB and FireWire as
       removable, see above).
    */
    if(!removable) {
        const char *allowlisted_bus =
            bus_has_ancestry(blockdevpath, hotplug_buses);
        if(allowlisted_bus) {
            removable = 1;
            debug("Found that device %s belong to allowlisted bus %s\n",
                  blockdevpath, allowlisted_bus);
        } else
            debug("Device %s does not belong to any allowlisted bus\n", device);
    }
    free(blockdevpath);
    return removable;
}

int
device_removable(const char *device)
{
    int removable = device_removable_silent(device);

    if(!removable)
        fprintf(stderr, _("Error: device %s is not removable\n"), device);

    return removable;
}

/**
   Checks whether a given device is allowlisted in /etc/pmount.allow
   (or any other value the ALLOWLIST has).
   @param device : the device name
 */
int
device_allowlisted(const char *device)
{
    FILE *fwl;
    char line[1024];
    char *d;
    regex_t re;
    regmatch_t match[3];
    int result;
    /* (Vincent Fourmond 6/1/2009): Adding :, as it comes in often in
       device names */
    const char *allowlist_regex =
        "^[[:space:]]*([][:alnum:]/:_+.[*?-]+)[[:space:]]*(#.*)?$";

    fwl = fopen(ALLOWLIST, "r");
    if(!fwl)
        return 0;

    result = regcomp(&re, allowlist_regex, REG_EXTENDED);
    if(result) {
        regerror(result, &re, line, sizeof(line));
        fprintf(stderr,
                "Internal error: device_allowlisted(): could not compile "
                "regex: %s\n",
                line);
        exit(E_INTERNAL);
    }

    debug("device_allowlist: checking " ALLOWLIST "...\n");

    while(fgets(line, sizeof(line), fwl)) {
        /* ignore lines which are too long */
        if(strlen(line) == sizeof(line) - 1) {
            debug("ignoring invalid oversized line\n");
            continue;
        }

        if(!regexec(&re, line, 3, match, 0)) {
            line[match[1].rm_eo] = 0;
            d = line + match[1].rm_so;
            debug("comparing %s against allowlisted '%s'\n", device, d);
            if(!fnmatch(d, device, FNM_PATHNAME)) {
                debug("device_allowlisted(): %s matches, returning 1\n", d);
                fclose(fwl);
                return 1;
            } else {
                char *full_path;
                /* We use realpath on the specification in order to follow
                   symlinks. See bug #507038 */
                if((full_path = realpath(d, NULL))) {
                    if(!strcmp(device, full_path)) {
                        debug("device_allowlisted(): %s matches after "
                              "realpath expansion, returning 1\n",
                              d);
                        fclose(fwl);
                        free(full_path);
                        return 1;
                    }
                    free(full_path);
                }
            }
        }
    }

    fclose(fwl);
    debug("device_allowlisted(): nothing matched, returning 0\n");
    return 0;
}

int
device_locked(const char *device)
{
    char *lockdirpath = make_lock_path(LOCKDIR, device);
    int locked = is_dir(lockdirpath);
    if(locked)
        fprintf(stderr, _("Error: device %s is locked\n"), device);
    free(lockdirpath);
    return locked;
}

int
mntpt_valid(const char *mntpt)
{
    char *fstab_device;
    int rc = 0;
    if(fstab_has_mntpt("/etc/fstab", mntpt, &fstab_device)) {
        fprintf(stderr,
                _("Error: mount point %s is already in /etc/fstab, "
                  "associated to device %s\n"),
                mntpt, fstab_device);
        free(fstab_device);
    } else {
        int fd = assert_dir(mntpt, 1);
        if(fd >= 0) {
            rc = assert_emptydir(fd);
            close(fd);
        }
    }

    return rc;
}

int
mntpt_mounted(const char *mntpt, int expect)
{
    int mounted = fstab_has_mntpt("/etc/mtab", mntpt, NULL) ||
                  fstab_has_mntpt("/proc/mounts", mntpt, NULL);

    if(mounted && !expect)
        fprintf(
            stderr,
            _("Error: directory %s already contains a mounted file system\n"),
            mntpt);
    else if(!mounted && expect)
        fprintf(
            stderr,
            _("Error: directory %s does not contain a mounted file system\n"),
            mntpt);

    return mounted;
}

static int
device_valid_silent(const char *device)
{
    struct stat st;

    if(stat(device, &st)) {
        return 0;
    }

    if(!S_ISBLK(st.st_mode)) {
        return 0;
    }

    return 1;
}

#define PROC_MOUNTS "/proc/mounts"
#define safe_strcpy(dest, src) snprintf(dest, sizeof(dest), "%s", src);

void
print_mounted_removable_devices(void)
{
    FILE *f;
    struct mntent *ent;

    if(!(f = setmntent(PROC_MOUNTS, "r"))) {
        fprintf(stderr, _("Error: could not open the %s file: %s"), PROC_MOUNTS,
                strerror(errno));
        exit(E_INTERNAL);
    }

    while((ent = getmntent(f)) != NULL) {
        if(device_valid_silent(ent->mnt_fsname) &&
           device_removable_silent(ent->mnt_fsname))
            printf("%s on %s type %s (%s)\n", ent->mnt_fsname, ent->mnt_dir,
                   ent->mnt_type, ent->mnt_opts);
    }
    endmntent(f);
}

/**
   Checks if the user is physically logged in, by looking for an utmp
   record pointing to a real tty.
*/
int
user_physically_logged_in(void)
{
    /* First, get the user name */
    char username[100];
    int retval = 0;
    struct passwd *pw;
    pw = getpwuid(getuid());
    if(!pw || pw->pw_uid != getuid()) {
        fputs(_("Impossible to find passwd record for current user\n"), stderr);
        exit(E_INTERNAL);
    }
    safe_strcpy(username, pw->pw_name);

    /* Then parse the utmpx database  */
    struct utmpx *s;
    setutxent(); /* rewind */
    while((s = getutxent())) {
        if(s->ut_type != USER_PROCESS)
            continue;
        if(!strncmp(s->ut_user, username, sizeof(s->ut_user))) {
            if(!strncmp(s->ut_line, "tty", 3) && isdigit(s->ut_line[3])) {
                /* Logged to a tty ! */
                retval = 1;
                break;
            }
        }
    }
    endutxent();
    return retval;
}

void
ensure_user_physically_logged_in(const char *progname)
{
    /* Check if the user is physically logged in */
    if(conffile_allow_not_physically_logged())
        return;
    if(user_physically_logged_in())
        return;
    fprintf(stderr,
            _("You are not physically logged in and your "
              "system administrator does not "
              "allow remote users to run %s, aborting\n"),
            progname);
    exit(E_DISALLOWED);
}
