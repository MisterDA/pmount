/**
 * utils.c - helper functions for pmount
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * Copyright 2004 Canonical Ltd.
 * Copyright 2009-2011 Vincent Fourmond
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <unistd.h>

#include "utils.h"

/* Error codes */
const int E_ARGS = 1;
const int E_DEVICE = 2;
const int E_MNTPT = 3;
const int E_POLICY = 4;
const int E_EXECMOUNT = 5;
const int E_EXECUMOUNT = 5;
const int E_UNLOCK = 6;
const int E_PID = 7;
const int E_LOCKED = 8;
const int E_DISALLOWED = 9;
const int E_LOSETUP = 10;
const int E_INTERNAL = 100;

/* File name used to tag directories created by pmount */
#define CREATED_DIR_STAMP ".created_by_pmount"

int enable_debug = 0;

int
debug(const char *format, ...)
{
    va_list va;
    int result;

    if(!enable_debug)
        return 0;

    va_start(va, format);
    result = vprintf(format, va);
    va_end(va);

    return result;
}

char *
strreplace(const char *s, char from, char to)
{
    char *result = strdup(s);

    if(!result) {
        fputs(_("Error: out of memory\n"), stderr);
        exit(E_INTERNAL);
    }

    char *i;
    for(i = result; *i; ++i)
        if(*i == from)
            *i = to;

    return result;
}

int
read_number_colon_number(const char *file, unsigned char *first,
                         unsigned char *second)
{
    FILE *f;
    char buf[100];
    int bufsize;
    unsigned int n1, n2;

    /* read a chunk from the file that is big enough to hold any two numbers */
    f = fopen(file, "r");
    if(!f)
        return -1;

    bufsize = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[bufsize] = 0;

    if(sscanf(buf, "%u:%u", &n1, &n2) != 2)
        return -1;

    if(n1 > 255 || n2 > 255)
        return -1;

    *first = (unsigned char)n1;
    *second = (unsigned char)n2;
    return 0;
}

int
assert_dir(const char *dir, int create_stamp)
{
    struct stat st;

    if(stat(dir, &st)) {
        int result;
        /* does not exist, create as root:root */
        get_root();
        get_groot();
        result = mkdir(dir, 0755);
        drop_groot();
        drop_root();
        if(result) {
            perror(_("Error: could not create directory"));
            return -1;
        }

        if(create_stamp) {
            char *stampfname;
            int stampfile;
            /* create stamp file to indicate that the directory should be
             * removed again at unmounting */
            if(asprintf(&stampfname, "%s/" CREATED_DIR_STAMP, dir) == -1) {
                perror("asprintf");
                return -1;
            }

            get_root();
            get_groot();
            stampfile = open(stampfname, O_CREAT | O_WRONLY | O_EXCL, 0600);
            drop_groot();
            drop_root();

            if(stampfile < 0) {
                perror(_("Error: could not create stamp file in directory"));
                return -1;
            }
            close(stampfile);
            free(stampfname);
        }
    } else {
        /* exists, check that it is a directory */
        if(!S_ISDIR(st.st_mode)) {
            fprintf(stderr, _("Error: %s is not a directory\n"), dir);
            return -1;
        }
    }

    return 0;
}

int
assert_emptydir(const char *dirname)
{
    DIR *dir;
    struct dirent *dirent;

    /* we might need root for reading the dir */
    get_root();
    dir = opendir(dirname);
    drop_root();

    if(!dir) {
        perror(_("Error: could not open directory"));
        return -1;
    }

    while((dirent = readdir(dir))) {
        if(strcmp(dirent->d_name, ".") != 0 &&
           strcmp(dirent->d_name, "..") != 0 &&
           strcmp(dirent->d_name, CREATED_DIR_STAMP) != 0) {
            closedir(dir);
            fprintf(stderr, _("Error: directory %s is not empty\n"), dirname);
            return -1;
        }
    }

    closedir(dir);
    return 0;
}

int
is_dir(const char *path)
{
    struct stat st;

    if(stat(path, &st))
        return 0;

    return S_ISDIR(st.st_mode);
}

int
is_block(const char *path)
{
    struct stat st;

    if(stat(path, &st))
        return 0;

    return S_ISBLK(st.st_mode);
}

int
remove_pmount_mntpt(const char *path)
{
    char *stampfile;
    int result = 0;

    if(asprintf(&stampfile, "%s/" CREATED_DIR_STAMP, path) == -1) {
        perror("asprintf");
        return -1;
    }

    get_root();
    if(!unlink(stampfile))
        result = rmdir(path);
    drop_root();
    free(stampfile);
    return result;
}

unsigned
parse_unsigned(const char *s, int exitcode)
{
    char *endptr;
    long int result;

    /* return 0 on NULL or empty strings */
    if(!s || !*s)
        return 0;

    errno = 0;
    result = strtol(s, &endptr, 0);
    if(*endptr == 0 && errno == 0 && result >= 0)
        return (unsigned)result;

    fprintf(stderr, _("Error: '%s' is not a valid number\n"), s);
    exit(exitcode);
}

int
pid_exists(unsigned pid)
{
    int result;

    get_root();
    result = kill(pid, 0);
    drop_root();

    return (result == 0) ? 1 : 0;
}

int
is_word_str(const char *s)
{
    const char *i;

    /* NULL or empty? */
    if(!s || !*s)
        return 0;

    for(i = s; *i; ++i) {
        if((*i >= 'a' && *i <= 'z') || (*i >= 'A' && *i <= 'Z') ||
           (*i >= '0' && *i <= '9') || (*i == '-') || (*i == '_'))
            continue;
        return 0;
    }

    return 1;
}

bool
check_root(void)
{
    return geteuid() == 0;
}

void
get_root(void)
{
    uid_t ruid, euid, suid;
    if(getresuid(&ruid, &euid, &suid) < 0) {
        perror("getresuid");
        exit(E_INTERNAL);
    }
    if(setresuid(-1, suid, -1) < 0) {
        perror("setresuid");
        exit(E_INTERNAL);
    }
    if(geteuid() != suid) {
        fputs(_("Internal error: could not change to effective uid root.\n"),
              stderr);
        exit(E_INTERNAL);
    }
}

void
drop_root(void)
{
    uid_t ruid = getuid();
    if(setresuid(-1, ruid, -1) < 0) {
        perror("setresuid");
        exit(E_INTERNAL);
    }
    if(geteuid() != ruid) {
        fputs(_("Internal error: could not change effective user id to real "
                "user id.\n"), stderr);
        exit(E_INTERNAL);
    }
}

void
drop_root_permanently(void)
{
    uid_t new_uid = getuid();
    uid_t ruid, euid, suid;
    gid_t new_gid = getgid();
    gid_t rgid, egid, sgid;

    if(setresuid(-1, new_uid, new_uid) < 0) {
        perror("setresuid");
        exit(E_INTERNAL);
    }
    if(getresuid(&ruid, &euid, &suid) < 0) {
        perror("getresuid");
        exit(E_INTERNAL);
    }
    if(ruid != new_uid || euid != new_uid || suid != new_uid) {
        fputs(_("Internal error: could not change effective user id to real "
                "user id.\n"), stderr);
        exit(E_INTERNAL);
    }

    if(setresgid(-1, new_gid, new_gid) < 0) {
        perror("setresgid");
        exit(E_INTERNAL);
    }
    if(getresgid(&rgid, &egid, &sgid) < 0) {
        perror("getresgid");
        exit(E_INTERNAL);
    }
    if(rgid != new_gid || egid != new_gid || sgid != new_gid) {
        fputs(_("Internal error: could not change effective group id to real "
                "group id.\n"), stderr);
        exit(E_INTERNAL);
    }
}

void
get_groot(void)
{
    gid_t rgid, egid, sgid;
    if(getresgid(&rgid, &egid, &sgid) < 0) {
        perror("getresgid");
        exit(E_INTERNAL);
    }
    if(setresgid(-1, sgid, -1) < 0) {
        perror("setresgid");
        exit(E_INTERNAL);
    }
    if(getegid() != sgid) {
        fputs(_("Internal error: could not change to effective gid root.\n"),
              stderr);
        exit(E_INTERNAL);
    }
}

void
drop_groot(void)
{
    gid_t rgid = getgid();
    if(setresgid(-1, rgid, -1) < 0) {
        perror("setresgid");
        exit(E_INTERNAL);
    }
    if(getegid() != rgid) {
        fputs(_("Internal error: could not change effective group id to real "
                "group id.\n"), stderr);
        exit(E_INTERNAL);
    }
}

int
spawnl(int options, const char *path, ...)
{
    char *argv[SPAWNL_ARG_MAX];
    unsigned argv_size = 0;
    va_list args;

    /* copy varargs to array */
    va_start(args, path);
    for(;;) {
        if(argv_size >= SPAWNL_ARG_MAX) {
            fprintf(stderr, "Internal error: spawnl(): too many arguments\n");
            exit(E_INTERNAL);
        }

        if((argv[argv_size++] = va_arg(args, char *)) == NULL)
            break;
    }
    va_end(args);

    return spawnv(options, path, argv);
}

char slurp_buffer[2048];

size_t slurp_size = 0;

#define DEVNULL_MASK (SPAWN_NO_STDOUT | SPAWN_NO_STDERR)
#define SLURP_MASK (SPAWN_SLURP_STDOUT | SPAWN_SLURP_STDERR)

int
spawnv(int options, const char *path, char *const argv[])
{
    int status;
    pid_t new_pid;
    int fds[2];

    if((options & SLURP_MASK) && pipe(fds)) {
        perror(_("Impossible to setup pipes for subprocess communication"));
        exit(E_INTERNAL);
    }

    if(enable_debug) {
        printf("spawnv(): executing %s", path);
        for(int i = 0; argv[i]; ++i)
            printf(" '%s'", argv[i]);
        printf("\n");
    }

    new_pid = fork();
    if(new_pid == -1) {
        perror(_("Impossible to fork"));
        exit(E_INTERNAL);
    }

    if(!new_pid) {
        if(options & SPAWN_EROOT)
            get_root();
        if(options & SPAWN_RROOT)
            if(setreuid(0, -1)) {
                perror(_("Error: could not raise to full root uid privileges"));
                exit(E_INTERNAL);
            }

        /* Performing redirections */

        if(options & DEVNULL_MASK) {
            int devnull = open("/dev/null", O_WRONLY);
            if(devnull != -1) {
                if(options & SPAWN_NO_STDOUT)
                    dup2(devnull, 1);
                if(options & SPAWN_NO_STDERR)
                    dup2(devnull, 2);
                close(devnull); /* Now useless */
            } else {
                perror("open(\"/dev/null\")");
                exit(E_INTERNAL);
            }
        }
        if(options & SLURP_MASK) {
            close(fds[0]); /* Close the read end of the pipe */

            if(options & SPAWN_SLURP_STDOUT)
                dup2(fds[1], 1);
            if(options & SPAWN_SLURP_STDERR)
                dup2(fds[1], 2);
            close(fds[1]); /* Now useless */
        }

        if(options & SPAWN_SEARCHPATH)
            execvp(path, argv);
        else
            execv(path, argv);
        exit(E_INTERNAL);
    } else {

        /* First, slurp all data */
        if(options & SLURP_MASK) {
            close(fds[1]); /* We don't need it */
            int nb_read = 0;
            slurp_size = 0;
            do {
                nb_read = read(fds[0], slurp_buffer + slurp_size,
                               sizeof(slurp_buffer) - 1 - slurp_size);
                if(nb_read < 0) {
                    perror(_("Error while reading from child process"));
                    exit(E_INTERNAL);
                }
                slurp_size += nb_read;
                if(slurp_size == sizeof(slurp_buffer) - 1)
                    break;
            } while(nb_read);

            if(nb_read) {
                fputs(_("Child process output has exceeded buffer size, please "
                        "file a bug report"),
                      stderr);
            }
            close(fds[0]); /* We close the reading end of the pipe */
            slurp_buffer[slurp_size] = 0; /* Make it nul-terminated */
        }

        if(wait(&status) < 0) {
            perror("Error: could not wait for executed subprocess");
            exit(E_INTERNAL);
        }
    }

    if(!WIFEXITED(status)) {
        fprintf(stderr,
                "Internal error: spawn(): process did not return a status");
        exit(E_INTERNAL);
    }

    status = WEXITSTATUS(status);
    debug("spawn(): %s terminated with status %i\n", path, status);
    return status;
}

/**
 * Internal function to determine lock file path for a given directory.
 */
static char *
get_dir_lockfile(const char *dir)
{
    char *name = NULL, *dir_fname;
    dir_fname = strreplace(dir, '/', '_');
    if(asprintf(&name, "/var/lock/pmount_%s", dir_fname) == -1)
        perror("asprintf");
    free(dir_fname);
    return name;
}

int
lock_dir(const char *dir)
{
    int f;
    char *lockfile = get_dir_lockfile(dir);
    if(!lockfile)
        return -1; /* name too long */

    get_root();
    f = creat(lockfile, 0600);
    drop_root();
    free(lockfile);
    if(f < 0) {
        perror("lock_dir(): creat");
        return -1;
    }

    if(lockf(f, F_TLOCK, 0) == 0)
        return 0;

    if(errno != EAGAIN)
        perror("lock_dir(): lockf");
    return -1;
}

void
unlock_dir(const char *dir)
{
    int f;
    char *lockfile = get_dir_lockfile(dir);
    if(!lockfile)
        return; /* name too long */

    get_root();
    f = open(lockfile, O_WRONLY);
    drop_root();
    if(f < 0) {
        if(errno != ENOENT)
            perror("unlock_dir(): open");
        return;
    }

    if(lockf(f, F_ULOCK, 0) != 0)
        perror("unlock_dir(): lockf");

    get_root();
    unlink(lockfile);
    drop_root();
}
