/**
 * utils.h - helper functions for pmount
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2004 Canonical Ltd.
 *
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#ifndef __utils_h
#define __utils_h

/* gettext abbreviation */
#define _(String) gettext(String)

/* global flag whether to print debug messages (false by default) */
extern int enable_debug;

/* printf() wrapper, only does anything if enable_debug != 0 */
int debug( const char* format, ... );

/**
 * Return a copy of string s with each occurrence of char 'from' replaced by char 
 * 'to'. Exit program immediately if out of memory.
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
 @ return 1 = directory, 0 = no directory
 */
int is_dir( const char* path );

/**
 * Remove a mountpoint created by pmount (i. e. only if the directory contains
 * a stamp file).
 * @return 0 on success, -1 on error
 */
int remove_pmount_mntpt( const char *path );

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
void get_root();

/**
 * Change effective user id back to getuid(). If this fails, print an error
 * message and exit with status 100.
 */
void drop_root();

/**
 * Change effective group id to root. If this fails, print an error message and
 * exit with status 100.
 */
void get_groot();

/**
 * Change effective group id back to getgid(). If this fails, print an error
 * message and exit with status 100.
 */
void drop_groot();

#endif /* __utils_h */

