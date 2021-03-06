/**
 * pmount.c - policy wrapper around 'mount' to allow mounting removable devices
 *            for normal users
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2004 Canonical Ltd.
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#define _GNU_SOURCE
#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <langinfo.h>
#include <libintl.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fs.h"
#include "loop.h"
#include "luks.h"
#include "policy.h"
#include "utils.h"
/* Configuration file handling */
#include "configuration.h"

/* Enable autodetection if possible */
#ifdef HAVE_BLKID
#include <blkid.h>
#endif

extern const char *VERSION;

/**
 * Print some help.
 * @param exename Name of the executable (argv[0]).
 */
static void
usage(const char *exename)
{
    printf(_("Usage:\n\n%s [options] <device> [<label>]\n\n"
             "  Mount <device> to a directory below %s if policy requirements\n"
             "  are met (see pmount(1) for details). If <label> is given, the "
             "mount point\n"
             "  will be %s<label>, otherwise it will be %s<device>.\n"
             "  If the mount point does not exist, it will be created.\n\n"),
           exename, MEDIADIR, MEDIADIR, MEDIADIR);

    printf(_("%s --lock <device> <pid>\n"
             "  Prevent further pmounts of <device> until it is unlocked "
             "again. <pid>\n"
             "  specifies the process id the lock holds for. This allows one "
             "to lock a device\n"
             "  by several independent processes and avoids indefinite locks "
             "of crashed\n"
             "  processes (nonexistent pids are cleaned before attempting a "
             "mount).\n\n"),
           exename);

    printf(_("%s --unlock <device> <pid>\n"
             "  Remove the lock on <device> for process <pid> again.\n\n"),
           exename);
    puts(_(
        "Options:\n"
        "  -r          : force <device> to be mounted read-only\n"
        "  -w          : force <device> to be mounted read-write\n"
        "  -s, --sync  : mount <device> with the 'sync' option (default: "
        "'async')\n"
        "  -A, --noatime\n"
        "                mount <device> with the 'noatime' option (default: "
        "'atime')\n"
        "  -e, --exec  : mount <device> with the 'exec' option (default: "
        "'noexec')\n"
        "  -t <fs>     : mount as file system type <fs> (default: "
        "autodetected)\n"
        "  -c <charset>: use given I/O character set (default: 'utf8' if "
        "called\n"
        "                in an UTF-8 locale, otherwise mount default)\n"
        "  -u <umask>  : use specified umask instead of the default (only for\n"
        "                file systems which actually support umask setting)\n"
        "  --fmask <fmask>\n"
        "                use specified fmask\n"
        "  --dmask <dmask>\n"
        "                use specified dmask\n"
        "  -p <file>, --passphrase <file>\n"
        "                read passphrase from file instead of the terminal\n"
        "                (only for LUKS encrypted devices)\n"
        "  -o          : mount fs with SELinux context "
        "system_u:object_r:removable_t:s0\n"
        "  -d, --debug : enable debug output (very verbose)\n"
        "  -F, --fsck  : runs fsck on the device before mounting\n"
        "  -h, --help  : print this help message and exit successfully\n"
        "  -V, --version\n"
        "                print version number and exit successfully"));
}

static struct {
    enum { MOUNT, LOCK, UNLOCK } mode;
    char *iocharset;
    char *umask, *fmask, *dmask;
    char *passphrase;
    char *use_fstype;
    bool exec;
    bool noatime;
    bool run_fsck; /* Whether or not to run fsck before mounting. */
    bool async;
    bool use_selinux_context;
    /* Whether the timestamps are stored in UTC rather than local time */
    bool utc;
    enum { FW_DEFAULT, FW_RO, FW_RW } force_write;
} options = {
    .mode = MOUNT,
    .iocharset = NULL,
    .umask = NULL,
    .fmask = NULL,
    .dmask = NULL,
    .passphrase = NULL,
    .use_fstype = NULL,
    .exec = false,
    .noatime = false,
    .run_fsck = false,
    .async = true,
    .use_selinux_context = false,
    .utc = false,
    .force_write = FW_DEFAULT,
};

/**
 * Check whether the user is allowed to mount the given device to the given
 * mount point. Creates the mount point if it does not exist yet.
 * @return 0 on success, -1 on failure
 */
static int
check_mount_policy(const char *device, const char *mntpt, int doing_loop)
{
    int result = device_valid(device) && !device_mounted(device, 0, NULL) &&
                 (doing_loop || device_allowlisted(device) ||
                  device_removable(device)) &&
                 !device_locked(device) && mntpt_valid(mntpt) &&
                 !mntpt_mounted(mntpt, 0);

    if(result)
        debug("policy check passed\n");
    else
        debug("policy check failed\n");

    /* the policy functions deliver booleans, but we want a standard Unix
       result */
    return result ? 0 : -1;
}

/**
 * Create a mount point pathname.
 * @param device device for which a moint point is created
 * @param label if NULL, the mount point will be MEDIADIR/device/, otherwise
 *        MEDIADIR/label/
 * @return the mount point pathname on success, NULL on failure
 */
static char *
make_mountpoint_name(const char *device, const char *label)
{
    char *mntpt = NULL;
    if(label) {
        /* ignore a leading MEDIADIR */
        if(!strncmp(label, MEDIADIR, sizeof(MEDIADIR) - 1))
            label += sizeof(MEDIADIR) - 1;

        if(!*label) {
            fputs(_("Error: label must not be empty\n"), stderr);
            return NULL;
        }
        if(strlen(label) > MAX_LABEL_SIZE) {
            fputs(_("Error: label too long\n"), stderr);
            return NULL;
        }

        if(strchr(label, '/')) {
            fputs(_("Error: '/' must not occur in label name\n"), stderr);
            return NULL;
        }

        if(asprintf(&mntpt, "%s%s/", MEDIADIR, label) == -1) {
            perror("asprintf");
            return NULL;
        }
    } else {
        char *d;
        if(strlen(device) > MAX_LABEL_SIZE) {
            fputs(_("Error: device name too long\n"), stderr);
            return NULL;
        }

        /* chop the DEVDIR prefix */
        if(!strncmp(device, DEVDIR, sizeof(DEVDIR) - 1))
            device += sizeof(DEVDIR) - 1;

        /* get rid of slashes */
        d = strreplace(device, '/', '_');
        if(asprintf(&mntpt, "%s%s/", MEDIADIR, d) == -1) {
            perror("asprintf");
            mntpt = NULL;
        }
        free(d);
    }

    if(mntpt != NULL)
        debug("mount point to be used: %s\n", mntpt);
    return mntpt;
}

/**
 * Drop all privileges and exec 'mount device'. Does not return on success, if
 * it returns, MOUNTPROG could not be executed.
 */
static void
do_mount_fstab(const char *device)
{
    debug("device %s handled by fstab, calling mount\n", device);

    /* drop all privileges and transparently call mount */
    get_root();
    get_groot();
    drop_root_permanently();

    execl(MOUNTPROG, MOUNTPROG, device, NULL);
    perror(_("Error: could not execute mount"));
}

/**
 * Raise to full privileges and call mount with given file system. Exits the
 * program immediately if MOUNTPROG cannot be executed or the given file system
 * is invalid. NOTE: This function must not exit() since it is called in a
 * lock-unlock-block.
 * @param device device node to mount
 * @param mntpt desired mount point
 * @param fsname file system name (mount option -t)
 * @param utf8 is true if the option utf8 should be used for VFAT
 * @param suppress_errors: if true, stderr is redirected to /dev/null
 * @return exit status of mount, or -1 on failure.
 */
static int
do_mount(const char *device, const char *mntpt, const char *fsname, int utf8,
         int suppress_errors)
{
    const struct FS *fs;
    char ugid_opt[100];
    char umask_opt[100];
    char fdmask_opt[100];
    char iocharset_opt[100];
    const char *utc_opt = "";
    const char *sync_opt = ",sync";
    const char *atime_opt = ",atime";
    const char *exec_opt = ",noexec";
    const char *access_opt = NULL;
    const char *selinux_context_opt = "";
    char mount_opts[1000];

    /* check and retrieve option information for requested file system */
    if(!fsname) {
        fputs(_("Internal error: mount_attempt: given file system name is "
                "NULL\n"),
              stderr);
        return -1;
    }

    fs = get_fs_info(fsname);
    if(!fs) {
        fprintf(stderr, _("Error: invalid file system name '%s'\n"), fsname);
        return -1;
    }

    /* validate user specified masks */
    if(options.umask && parse_unsigned(options.umask, E_ARGS) > 0777) {
        fprintf(stderr, _("Error: invalid umask %s\n"), options.umask);
        return -1;
    }

    if(options.fmask && parse_unsigned(options.fmask, E_ARGS) > 0777) {
        fprintf(stderr, _("Error: invalid fmask %s\n"), options.fmask);
        return -1;
    }

    if(options.dmask && parse_unsigned(options.dmask, E_ARGS) > 0777) {
        fprintf(stderr, _("Error: invalid dmask %s\n"), options.dmask);
        return -1;
    }

    /* assemble option string */
    *ugid_opt = *umask_opt = *fdmask_opt = *iocharset_opt = 0;
    if(fs->support_ugid) {
        struct stat statbuf;
        gid_t gid = getgid();
        int result;

        /* if pmount is installed setgid, use that group, otherwise use the
         * user's group */
        get_root();
        result = stat("/proc/self/exe", &statbuf);
        drop_root();
        if(result < 0)
            fprintf(stderr, "Can't stat myself\n");
        else {
            if(statbuf.st_mode & S_ISGID)
                gid = statbuf.st_gid;
        }
        snprintf(ugid_opt, sizeof(ugid_opt), ",uid=%u,gid=%u", getuid(), gid);
    }

    if(fs->umask)
        snprintf(umask_opt, sizeof(umask_opt), ",umask=%s",
                 options.umask ? options.umask : fs->umask);
    /* If the fs supports fdmasks, we try to make some values
       up.
    */
    if(fs->umask && fs->fdmask) {
        /* We deal with masks in another way now: */
        unsigned i_umask, i_dmask, i_fmask;

        /* We first get the umask value */
        if(options.umask)
            i_umask =
                parse_unsigned(options.umask, E_ARGS); /* shouldn't fail */
        else
            i_umask = parse_unsigned(fs->umask, E_ARGS); /* shouldn't fail */

        /* Now the fmask */
        if(options.fmask)
            i_fmask =
                parse_unsigned(options.fmask, E_ARGS); /* shouldn't fail */
        else                          /* make up from the umask parameter */
            i_fmask = i_umask | 0111; /* remove exec permissions */
        /* And the dmask */
        if(options.dmask)
            i_dmask =
                parse_unsigned(options.dmask, E_ARGS); /* shouldn't fail */
        else                   /* make up from the umask parameter */
            i_dmask = i_umask; /* same as umask */
        snprintf(fdmask_opt, sizeof(fdmask_opt), fs->fdmask, i_fmask, i_dmask);
    }

    if(options.async)
        sync_opt = ",async";

    if(options.noatime)
        atime_opt = ",noatime";

    if(options.exec)
        exec_opt = ",exec";

    if(options.force_write == FW_RO)
        access_opt = ",ro";
    else if(options.force_write == FW_RW)
        access_opt = ",rw";
    else
        access_opt = "";

    if(options.use_selinux_context)
        selinux_context_opt = ",context=system_u:object_r:removable_t:s0";

    if(!strcmp(fsname, "vfat") && options.utc)
        utc_opt = ",tz=UTC";

    if(options.iocharset && fs->iocharset_format) {
        if(!is_word_str(options.iocharset)) {
            fprintf(stderr, _("Error: invalid charset name '%s'\n"),
                    options.iocharset);
            return -1;
        }
        /* VFAT and UTF-8 need special care, see bug #443514 and mount(1) */
        if(!strcmp(fsname, "vfat") && utf8) {
            debug("VFAT in a UTF-8 locale: using option utf8\n");
            if(!strcmp(options.iocharset, "utf8")) {
                debug(
                    "filesystem is vfat and charset is utf-8: using iso8859-1\n"
                    "You can change with the -c option");
                snprintf(iocharset_opt, sizeof(iocharset_opt),
                         ",utf8,iocharset=iso8859-1");
            } else {
                snprintf(iocharset_opt, sizeof(iocharset_opt),
                         ",utf8,iocharset=%s", options.iocharset);
            }
        } else {
            snprintf(iocharset_opt, sizeof(iocharset_opt), fs->iocharset_format,
                     options.iocharset);
        }
    } else if(!strcmp(fsname, "vfat") && fs->iocharset_format) {
        /* We still make a special case for vfat, as in certain cases,
           mount will mount it with iocharset=utf8, some times without
           warning. So, in the absence of a specified charset, we
           force iocharset=iso8859-1*/
        snprintf(iocharset_opt, sizeof(iocharset_opt), fs->iocharset_format,
                 "iso8859-1");
    }

    snprintf(mount_opts, sizeof(mount_opts), "%s%s%s%s%s%s%s%s%s%s%s",
             fs->options, sync_opt, atime_opt, exec_opt, access_opt, ugid_opt,
             umask_opt, fdmask_opt, iocharset_opt, utc_opt,
             selinux_context_opt);

    /* go for it */
    return spawnl(SPAWN_EROOT | SPAWN_RROOT |
                      (suppress_errors ? SPAWN_NO_STDERR : 0),
                  MOUNTPROG, MOUNTPROG, "-t", fsname, "-o", mount_opts, device,
                  mntpt, (char *)NULL);
}

/**
 * Try to call do_mount() with every supported file system until a call
 * succeeds.
 * @param device device node to mount
 * @param mntpt desired mount point
 * @param utf8 is true if the option utf8 should be used for VFAT
 * @return last return value of do_mount (i. e. 0 on success, != 0 on error)
 */
static int
do_mount_auto(const char *device, const char *mntpt, int utf8)
{
    const struct FS *fs;
    int nostderr = 1;
    int result = -1;
    struct stat buf; /* Not used */

    /* First, if that is supported, we try with blkid */
#ifdef HAVE_BLKID
    const char *tp;
    blkid_cache c;
    blkid_get_cache(&c, "/dev/null");
    get_root();
    tp = blkid_get_tag_value(c, "TYPE", device);
    drop_root();
    if(tp) {
        debug("blkid gave FS %s for '%s'\n", tp, device);
        if(!strcmp(tp, "ntfs") && !stat(MOUNT_NTFS_3G, &buf)) {
            debug(
                "blkdid detected ntfs and ntfs-3g was found. Using ntfs-3g\n");
            tp = "ntfs-3g";
        }
        result = do_mount(device, mntpt, tp, utf8, nostderr);
        if(result == 0)
            return result;
        debug("blkid-detected FS failed, trying manually \n");
    }
    blkid_put_cache(c);
#endif /* HAVE_BLKID */

    result = -1;

    for(fs = get_supported_fs(); fs->fsname; ++fs) {
        /* Skip fs marked as such unless it is ntfs-3g and
           we can stat MOUNT_NTFS_3G
        */
        if(fs->skip_autodetect &&
           !(!strcmp(fs->fsname, "ntfs-3g") && !stat(MOUNT_NTFS_3G, &buf)))
            continue; /* skip fs that are marked as such */
        /* don't suppress stderr if we try the last possible fs */
        if((fs + 1)->fsname == NULL)
            nostderr = 0;
        result = do_mount(device, mntpt, fs->fsname, utf8, nostderr);
        if(result == 0)
            break;

        /* sometimes VFAT fails when using iocharset; try again without */
        if(options.iocharset)
            result = do_mount(device, mntpt, fs->fsname, utf8, nostderr);
        if(result <= 0)
            break;
    }
    return result;
}

/**
 * Lock given device.
 * param pid pid of program that holds the lock
 * @return 0 on success, -1 on error (message is printed in this case).
 */
static int
do_lock(const char *device, pid_t pid)
{
    int lockdir_fd, lockdir_device_fd, pidlock_fd;
    char *lockdir_device_name, *pidlock_name;
    int rc = -1;

    lockdir_fd = assert_dir(LOCKDIR, 0);
    if(lockdir_fd < 0)
        return rc;
    lockdir_device_name = make_lock_name(device);
    if(lockdir_device_name == NULL)
        goto lockdir_fd;
    lockdir_device_fd = assert_dir_at(lockdir_fd, lockdir_device_name, 0);
    if(lockdir_device_fd < 0)
        goto lockdir_device_name;

    /* only allow to create locks for existing pids, to prevent DOS attacks */
    if(!pid_exists(pid)) {
        fprintf(
            stderr,
            _("Error: cannot lock for pid %u, this process does not exist\n"),
            pid);
        goto lockdir_device_fd;
    }

    if(asprintf(&pidlock_name, "%d", pid) == -1) {
        perror("asprintf");
        goto lockdir_device_fd;
    }

    /* we need root for creating the pid lock file */
    get_root();
    get_groot();
    pidlock_fd =
        openat(lockdir_device_fd, pidlock_name, O_WRONLY | O_CREAT, 0644);
    drop_groot();
    drop_root();

    if(pidlock_fd < 0) {
        fprintf(stderr, _("Error: could not create pid lock file %s: %s\n"),
                pidlock_name, strerror(errno));
        goto pidlock_name;
    }

    rc = 0;
    close(pidlock_fd);
pidlock_name:
    free(pidlock_name);
lockdir_device_fd:
    close(lockdir_device_fd);
lockdir_device_name:
    free(lockdir_device_name);
lockdir_fd:
    close(lockdir_fd);
    return rc;
}

/**
 * Unlock given device.
 * param pid pid of program that holds the lock
 * @return 0 on success, -1 on error (message is printed in this case).
 */
static int
do_unlock(const char *device, pid_t pid)
{
    char *lockdir_device_path = make_lock_path(LOCKDIR, device);
    int rc = -1;

    /* if no lock dir exists, device is not locked */
    if(!is_dir(lockdir_device_path))
        return 0;

    /* remove pid file first */
    if(pid) {
        char *pidlock_path;
        if(asprintf(&pidlock_path, "%s/%d", lockdir_device_path, pid) == -1) {
            perror("asprintf");
            goto lockdir_device_path;
        }

        /* we need root for removing the pid lock file */
        get_root();
        rc = unlink(lockdir_device_path);
        drop_root();

        /* ignore nonexistent lock files, but report other errors */
        if(rc && errno != ENOENT) {
            fprintf(stderr, _("Error: could not remove pid lock file %s: %s\n"),
                    pidlock_path, strerror(errno));
            free(pidlock_path);
            goto lockdir_device_path;
        }
        free(pidlock_path);
    }

    /* Try to rmdir the dir. If there are still files (pid-locks) in it, this
     * will fail. */
    get_root();
    rc = rmdir(lockdir_device_path);
    drop_root();

    if(rc) {
        if(errno == ENOTEMPTY)
            rc = 0;
        else
            perror(_("Error: do_unlock: could not remove lock directory"));
    }

lockdir_device_path:
    free(lockdir_device_path);
    return rc;
}

/**
 * Runs fsck on the device. Will fail if fsck returns an error code
 * greater than 1 (1 is fine, it just means that problems were
 * corrected).
 *
 * @return 0 on success, -1 on error.
 */
static int
do_fsck(const char *device)
{
    int result;
    debug("running fsck on %s\n", device);

    result = spawnl(SPAWN_EROOT | SPAWN_RROOT, FSCKPROG, FSCKPROG, "-C1",
                    device, (char *)NULL);
    if(result < -1) {
        perror(_("Error: could not execute fsck"));
        return -1;
    } else if(result > 1) {
        fputs(_("fsck returned error code above 1: "
                "something went wrong\n"),
              stderr);
        return -1;
    }
    /* Error code of 0 or 1 is fine. */
    return 0;
}

/**
 * Remove stale pid locks from device's lock directory.
 */
static void
clean_lock_dir(const char *device)
{
    char *lockdirpath = make_lock_path(LOCKDIR, device);
    DIR *lockdir;
    struct dirent *lockfile;

    debug("Cleaning lock directory %s\n", lockdirpath);

    get_root();
    lockdir = opendir(lockdirpath);
    drop_root();

    if(!lockdir)
        return;

    while((lockfile = readdir(lockdir))) {
        if(!strcmp(lockfile->d_name, ".") || !strcmp(lockfile->d_name, ".."))
            continue;

        debug("  checking whether %s is alive\n", lockfile->d_name);

        if(!pid_exists(parse_unsigned(lockfile->d_name, E_INTERNAL))) {
            char *lockfilepath;
            debug("  %s is dead, removing lock file\n", lockfile->d_name);
            if(asprintf(&lockfilepath, "%s/%s", lockdirpath,
                        lockfile->d_name) == -1) {
                perror("asprintf");
                continue;
            }
            get_root();
            unlink(lockfilepath);
            drop_root();
            free(lockfilepath);
        }
    }

    /* remove the directory if it got empty */
    get_root();
    rmdir(lockdirpath);
    drop_root();
    closedir(lockdir);
    free(lockdirpath);
}

/**
 * Entry point.
 */
int
main(int argc, char *argv[])
{
    char *devarg = NULL, *arg2 = NULL;
    char *device, *mntptdev, *decrypted_device;
    const char *fstab_device;
    int is_real_path = 0;
    int doing_loop_mount = 0;
    int utf8;
    int result;

    const struct option long_opts[] = {
        { "charset", 1, NULL, 'c' },
        { "debug", 0, NULL, 'd' },
        { "dmask", 1, NULL, 0 },
        { "exec", 0, NULL, 'e' },
        { "fmask", 1, NULL, 0 },
        { "fsck", 0, NULL, 'F' },
        { "help", 0, NULL, 'h' },
        { "lock", 0, NULL, 'l' },
        { "noatime", 0, NULL, 'A' },
        { "passphrase", 1, NULL, 'p' },
        { "read-only", 0, NULL, 'r' },
        { "read-write", 0, NULL, 'w' },
        { "selinux-context", 0, (int *)&options.use_selinux_context, true },
        { "sync", 0, NULL, 's' },
        { "type", 1, NULL, 't' },
        { "umask", 1, NULL, 'u' },
        { "unlock", 0, NULL, 'L' },
        { "utc", 0, (int *)&options.utc, true },
        { "version", 0, NULL, 'V' },
        { NULL, 0, NULL, 0 },
    };

    /* initialize locale */
    setlocale(LC_ALL, "");
    bindtextdomain("pmount", NULL);
    textdomain("pmount");

    /* If pmount is run without a single argument, print out the list
       of removable devices. Does not require root privileges, just read access
       to the /proc/mounts file.
    */
    if(argc == 1) {
        puts(_("Printing mounted removable devices:\n"));
        print_mounted_removable_devices();
        printf(_("To get a short help, run %s -h\n"), argv[0]);
        return 0;
    }

    /* parse command line options */
    while(1) {
        int option_index = 0,
            option = getopt_long(argc, argv, "+c:deFhlAp:rwst:u:LV", long_opts,
                                 &option_index);
        if(option == -1) /* end of arguments */
            break;
        switch(option) {
        case '?':
            return E_ARGS; /* unknown argument */
        case 0:
            if(strcmp(long_opts[option_index].name, "dmask") == 0)
                options.dmask = optarg;
            else if(strcmp(long_opts[option_index].name, "fmask") == 0)
                options.fmask = optarg;
            break;
        case 'A':
            options.noatime = true;
            break;
        case 'c':
            options.iocharset = optarg;
            break;
        case 'd':
            enable_debug = 1;
            break;
        case 'e':
            options.exec = true;
            break;
        case 'F':
            if(conffile_allow_fsck()) {
                options.run_fsck = true;
            } else {
                fputs(_("Your system administrator does not "
                        "allow users to run fsck, aborting\n"),
                      stderr);
                return E_DISALLOWED;
            }
            break;
        case 'h':
            usage(argv[0]);
            return EXIT_SUCCESS;
        case 'l':
            options.mode = LOCK;
            break;
        case 'L':
            options.mode = UNLOCK;
            break;
        case 'p':
            options.passphrase = optarg;
            break;
        case 'r':
            options.force_write = FW_RO;
            break;
        case 's':
            options.async = false;
            break;
        case 't':
            options.use_fstype = optarg;
            break;
        case 'u':
            options.umask = optarg;
            break;
        case 'V':
            puts(VERSION);
            return EXIT_SUCCESS;
        case 'w':
            options.force_write = FW_RW;
            break;
        default:
            fputs(_("Internal error: getopt_long() returned unknown value\n"),
                  stderr);
            return E_INTERNAL;
        }
    }

    /* determine device and second (label/pid) argument */
    if(optind < argc)
        devarg = argv[optind];

    if(optind + 1 < argc)
        arg2 = argv[optind + 1];

    /* check number of arguments */
    if(!devarg || (options.mode != MOUNT && !arg2) || argc > optind + 2) {
        usage(argv[0]);
        return E_ARGS;
    }

    if(conffile_system_read()) {
        fputs(_("Error while reading system configuration file\n"), stderr);
        return E_INTERNAL;
    }

    /* are we root? */
    if(!check_root()) {
        fputs(_("Error: this program needs to be installed suid root\n"),
              stderr);
        return E_INTERNAL;
    }

    /* drop root privileges until we really need them (still available as saved
     * uid) */
    drop_root();
    drop_groot();

    /* Check if the user is physically logged in */
    ensure_user_physically_logged_in(argv[0]);

    /* Lookup in /etc/fstab if devarg is a mount point, unless we already
       have a block device -- this way, pmount shouldn't choke on stale
       network mounts. */

    if(!is_block(devarg) && fstab_has_mntpt("/etc/fstab", devarg, &mntptdev)) {
        debug("resolved mount point %s to device %s\n", devarg, mntptdev);
        devarg = mntptdev;
    }

    /* get real path, if possible */
    if((device = realpath(devarg, NULL))) {
        debug("resolved %s to device %s\n", devarg, device);
        is_real_path = 1;
    } else {
        debug("realpath(%s): %s\n", devarg, strerror(errno));
        device = strdup(devarg);
        if(!device) {
            perror("strdup(devarg)");
            return E_INTERNAL;
        }
    }

    /* is the device already handled by fstab? We allow is_real_path == 0 here
     * to transparently mount things like NFS and SMB drives */
    fstab_device = fstab_has_device("/etc/fstab", device, NULL, NULL);
    if(options.mode == MOUNT && fstab_device) {
        if(arg2)
            fprintf(stderr,
                    _("Warning: device %s is already handled by /etc/fstab,"
                      " supplied label is ignored\n"),
                    fstab_device);

        do_mount_fstab(fstab_device);
        free(device);
        return E_EXECMOUNT;
    }

    if(is_real_path && (!is_block(device))) {
        char *loop_device;
        if(!conffile_allow_loop()) {
            fprintf(stderr,
                    _("You are trying to mount %s as a loopback device. \n"
                      "However, you are not allowed to use loopback mount.\n"),
                    devarg);
            free(device);
            return E_DISALLOWED;
        }
        if(loopdev_associate(device, &loop_device)) {
            fprintf(stderr, _("Failed to setup loop device for %s, aborting\n"),
                    devarg);
            free(device);
            return E_LOSETUP;
        }
        free(device);
        device = loop_device;
        /* For bypassing policy check afterwards, we've done
           everything already.
        */
        doing_loop_mount = 1;
    }

    /* pmounted devices really have to be a proper local device */
    if(!is_real_path) {
        /* try to prepend '/dev' */
        if(strncmp(device, DEVDIR, sizeof(DEVDIR) - 1) != 0) {
            char *dev_device, *realpath_dev_device;
            if(asprintf(&dev_device, "%s%s", DEVDIR, device)) {
                perror("asprintf");
                free(device);
                return E_INTERNAL;
            }
            if(!(realpath_dev_device = realpath(dev_device, NULL))) {
                fprintf(stderr, "realpath(%s): %s\n", dev_device,
                        strerror(errno));
                free(device);
                return E_DEVICE;
            }
            free(device);
            device = realpath_dev_device;
            debug("trying to prepend '" DEVDIR "' to device argument, now %s\n",
                  device);
            /* We need to lookup again in fstab: */
            fstab_device = fstab_has_device("/etc/fstab", device, NULL, NULL);
            if(options.mode == MOUNT && fstab_device) {
                if(arg2)
                    fprintf(
                        stderr,
                        _("Warning: device %s is already handled by /etc/fstab,"
                          " supplied label is ignored\n"),
                        fstab_device);

                do_mount_fstab(fstab_device);
                free(device);
                return E_EXECMOUNT;
            }
        }
    }

    /* does the device start with DEVDIR? */
    if(strncmp(device, DEVDIR, sizeof(DEVDIR) - 1) != 0) {
        fprintf(stderr, _("Error: invalid device %s (must be in /dev/)\n"),
                device);
        free(device);
        return E_DEVICE;
    }

    switch(options.mode) {
    case MOUNT: {
        /* determine mount point name; note that we use devarg instead of
         * device to preserve symlink names (like '/dev/usbflash' instead
         * of '/dev/sda1') */
        char *mntpt = make_mountpoint_name(devarg, arg2);
        if(mntpt == NULL) {
            if(doing_loop_mount)
                loopdev_dissociate(device);
            free(device);
            return E_MNTPT;
        }

        /* if no charset was set explicitly, autodetect UTF-8 */
        if(!options.iocharset) {
            const char *codeset;
            codeset = nl_langinfo(CODESET);

            debug("no iocharset given, current locale encoding is %s\n",
                  codeset);

            if(codeset && !strcmp(codeset, "UTF-8")) {
                debug("locale encoding uses UTF-8, setting iocharset to "
                      "'utf8'\n");
                options.iocharset = "utf8";
                utf8 = 1;
            } else {
                utf8 = 0;
            }
        } else {
            utf8 = strcmp(options.iocharset, "utf8") == 0;
        }

        /* clean stale locks */
        clean_lock_dir(device);

        if(check_mount_policy(device, mntpt, doing_loop_mount)) {
            if(doing_loop_mount)
                loopdev_dissociate(device);
            free(device);
            free(mntpt);
            return E_POLICY;
        }

        /*
           Here, we try to open the device, in order to check that
           for instance medium is present.
        */
#ifdef ENOMEDIUM
        get_root();
        int fd = open(device, O_RDONLY);
        if(fd == -1) {
            perror(_("Could not open device"));
            free(device);
            free(mntpt);
            return E_DEVICE;
        }
        drop_root();
#endif

        /* check for encrypted device */
        enum decrypt_status decrypt =
            luks_decrypt(device, &decrypted_device, options.passphrase,
                         options.force_write == FW_RO ? 1 : 0);

        switch(decrypt) {
        case DECRYPT_FAILED:
            fputs(_("Error: could not decrypt device (wrong passphrase?)\n"),
                  stderr);
            if(doing_loop_mount)
                loopdev_dissociate(device);
            free(device);
            free(mntpt);
            return E_POLICY;
        case DECRYPT_EXISTS:
            fputs(_("Error: mapped device already exists\n"), stderr);
            if(doing_loop_mount)
                loopdev_dissociate(device);
            free(device);
            free(mntpt);
            return E_POLICY;
        case DECRYPT_OK:
            /* We create a luks lockfile _on the decrypted device !_*/
            if(!luks_create_lockfile(decrypted_device))
                fputs(_("Warning: could not create luks lockfile\n"), stderr);
        case DECRYPT_NOTENCRYPTED:
            break;
        default:
            fprintf(stderr, "Internal error: unhandled decrypt_status %i\n",
                    (int)decrypt);
            free(device);
            free(mntpt);
            return E_INTERNAL;
        }

        /* lock the mount directory */
        debug("locking mount point directory\n");
        if(lock_dir(mntpt) < 0) {
            fputs(_("Error: could not lock the mount directory. Another pmount "
                    "is probably running for this mount point.\n"),
                  stderr);
            if(doing_loop_mount)
                loopdev_dissociate(device);
            free(device);
            free(mntpt);
            return E_LOCKED;
        }
        debug("mount point directory locked\n");

        /* Now starting fsck if requested. */
        if(options.run_fsck) {
            result = do_fsck(decrypted_device);
            if(result)
                fputs(_("Error: fsck failed, not mounting\n"), stderr);
        } else
            result = 0;

        /* Only mount if fsck went fine */
        if(!result) {
            /* off we go */
            if(options.use_fstype)
                result = do_mount(decrypted_device, mntpt, options.use_fstype,
                                  utf8, 0);
            else
                result = do_mount_auto(decrypted_device, mntpt, utf8);
        }

        /* unlock the mount point again */
        debug("unlocking mount point directory\n");
        unlock_dir(mntpt);
        debug("mount point directory unlocked\n");

        if(result) {
            if(decrypt == DECRYPT_OK)
                luks_release(decrypted_device, 0);

            if(doing_loop_mount)
                loopdev_dissociate(device);
            free(device);

            /* mount failed, delete the mount point again */
            if(remove_pmount_mntpt(mntpt)) {
                perror(_("Error: could not delete mount point"));
                free(mntpt);
                return EXIT_FAILURE;
            }
            free(mntpt);
            return E_EXECMOUNT;
        }
        free(device);
        free(mntpt);
        return EXIT_SUCCESS;
    }
    case LOCK:
        if(device_valid(device)) {
            if(do_lock(device, parse_unsigned(arg2, E_PID))) {
                free(device);
                return E_INTERNAL;
            }
        }
        free(device);
        return 0;

    case UNLOCK:
        if(device_valid(device)) {
            if(do_unlock(device, parse_unsigned(arg2, E_PID))) {
                free(device);
                return E_UNLOCK;
            }
        }
        free(device);
        return 0;
    }

    fprintf(stderr, _("Internal error: mode %s not handled.\n"),
            options.mode == MOUNT  ? "MOUNT"
            : options.mode == LOCK ? "LOCK"
                                   : "UNLOCK");
    free(device);
    return E_INTERNAL;
}
