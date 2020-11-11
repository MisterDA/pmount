/**
 * @file utils.h - helper functions for pmount
 *
 * Authors: Martin Pitt <martin.pitt@canonical.com>
 *          (c) 2004 Canonical Ltd.
 *          Vincent Fourmond <fourmond@debian.org>
 *          (c) 2007-2011 by Vincent Fourmond
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the
 * license.
 */

#ifndef __utils_h
#define __utils_h

/** 
 * gettext abbreviation 
 */
#define _(String) gettext(String)

/**
 * global flag whether to print debug messages (false by default) 
 */
extern int enable_debug;

/**
 * printf() wrapper, only does anything if enable_debug != 0 
 */
int debug( const char* format, ... ) __attribute__((format(printf, 1, 2)));

/**
 * Return a copy of string s with each occurrence of char 'from'
 * replaced by char 'to'. Exit program immediately if out of memory.
 */
char* strreplace( const char* s, char from, char to );

/**
 * If dir already exists, check that it is a directory; if it does not exist,
 * create it. If create_stamp is true, put a stamp file into it (so that it
 * will be removed again on unmounting).
 * @return 0 on success, -1 on error (message is printed in this case)
 */
int assert_dir( const char* dir, int create_stamp );

/**
 * Assert that given directory is empty.
 * @return 0 on success, -1 on error (message is printed in this case)
 */
int assert_emptydir( const char* dirname );

/**
 * Return whether given path is a directory.
 * @return 1 = directory, 0 = no directory
 */
int is_dir( const char* path );

/**
 * Return whether given path is a block device.
 * @return 1 = block device, 0 = no block device
 */
int is_block( const char* path );


/**
 * Remove a mountpoint created by pmount (i. e. only if the directory contains
 * a stamp file).
 * @return 0 on success, -1 on error
 */
int remove_pmount_mntpt( const char *path );

/**
 * Put a lock on the given mount point directory to avoid race conditions with
 * parallel pmount instances.
 * @return 0 on success, -1 if the directory is already locked by another
 *         pmount instance.
 */
int lock_dir( const char* dir );

/**
 * Unlock a directory that was locked by lock_dir().
 */
void unlock_dir (const char* dir );

/**
 * Read two numbers (separated by colon) from given file and return them in
 * first and second; allowed range is 0 to 255.
 * @return 0 on success, -1 on error
 */
int read_number_colon_number( const char* file, unsigned char* first, unsigned char* second );

/**
 * Parse s as nonnegative number. Exits the program immediately if s cannot be
 * parsed as a number.
 * @param s string to parse as a number
 * @param exitcode if s cannot be parsed, exit with this exit code
 * @return The parsed number if s is a valid number string; 0 if s is NULL.
 */
unsigned parse_unsigned( const char* s, int exitcode );

/**
 * Return whether a process with the given pid exists.
 * @return 1: pid exists, 0: pid does not exist
 */
int pid_exists( unsigned pid );

/**
 * Return 1 if s contains only ASCII letters (a-z, A-Z), digits (0-9), dashes
 * (-) and underscores (_). Return 0 if it is NULL, empty, or contains other
 * characters.
 */
int is_word_str( const char* s );

/**
 * Change effective user id to root. If this fails, print an error message and
 * exit with status 100.
 */
void get_root(void);

/**
 * Change effective user id back to getuid(). If this fails, print an
 * error message and exit with status 100.
 */
void drop_root(void);

/**
 * Change effective group id to root. If this fails, print an error
 * message and exit with status 100.
 */
void get_groot(void);

/**
 * Change effective group id back to getgid(). If this fails, print an
 * error message and exit with status 100.
 */
void drop_groot(void);

/* spawn() options */
#define SPAWN_EROOT 0x01
#define SPAWN_RROOT 0x02
#define SPAWN_NO_STDOUT 0x04
#define SPAWN_NO_STDERR 0x08
#define SPAWN_SEARCHPATH 0x10
#define SPAWN_SLURP_STDOUT 0x20
#define SPAWN_SLURP_STDERR 0x40

/**
   A buffer in which the slurped stdout/stderr are stored.

   It is nul-terminated. (although zeros may occur before the end, see
   slurp_size).
 */
extern char slurp_buffer[];

/**
   The size of the contents of slurp_buffer.
 */
extern size_t slurp_size;

/**
 * Synchronously spawn a subprocess and return its exit status.
 * @param options Combination of SPAWN_* flags
 * @param path Path to program to be executed
 * @param ... NULL terminated argument list (including argv[0]!)
 * @return The exit status of the program, or -1 if the program could not be
 *         executed.
 */
int spawnl( int options, const char* path, ... );

/**
 * Synchronously spawn a subprocess and return its exit status.
 * @param options Combination of SPAWN_* flags
 * @param path Path to program to be executed
 * @param argv NULL terminated argument vector (including argv[0]!)
 * @return The exit status of the program, or -1 if the program could not be
 *         executed.
 */
int spawnv( int options, const char* path, char *const argv[] );

#endif /* __utils_h */

