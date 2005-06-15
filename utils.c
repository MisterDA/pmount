/**
 * utils.c - helper functions for pmount
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2004 Canonical Ltd.
 *
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <libintl.h>

/* File name used to tag directories created by pmount */
#define CREATED_DIR_STAMP ".created_by_pmount"

int enable_debug = 0;

int
debug( const char* format, ... ) {
    va_list va;
    int result;

    if( !enable_debug )
        return 0;

    va_start( va, format );
    result = vprintf( format, va );
    va_end( va );

    return result;
}

char*
strreplace( const char* s, char from, char to )
{
    char* result = strdup( s );

    if( !result ) {
        fprintf( stderr, _("Error: out of memory\n") );
        exit( 100 );
    }

    char* i;
    for( i = result; *i; ++i )
        if( *i == from )
            *i = to;

    return result;
}

int
read_number_colon_number( const char* file, unsigned char* first, unsigned char* second )
{
    FILE* f;
    char buf[100];
    int bufsize;
    unsigned int n1, n2;
    
    /* read a chunk from the file that is big enough to hold any two numbers */
    f = fopen( file, "r" );
    if( !f )
        return -1;

    bufsize = fread( buf, 1, sizeof(buf)-1, f );
    fclose( f );
    buf[bufsize] = 0;

    if( sscanf( buf, "%u:%u", &n1, &n2 ) != 2 )
        return -1;

    if( n1 > 255 || n2 > 255 )
        return -1;

    *first = (unsigned char) n1;
    *second = (unsigned char) n2;
    return 0;
}

int
assert_dir( const char* dir )
{
    int result;
    struct stat st;
    int stampfile;
    char stampfname[PATH_MAX];

    if( stat( dir, &st ) ) {
        /* does not exist, create as root:root */
        get_root();
        get_groot();
        result = mkdir( dir, 0755 );
        drop_groot();
        drop_root();
        if( result ) {
            perror( _("Error: could not create directory") );
            return -1;
        }

        /* create stamp file to indicate that the directory should be removed
         * again at unmounting */
        snprintf( stampfname, sizeof( stampfname ), "%s/%s", dir, CREATED_DIR_STAMP );

        get_root();
        get_groot();
        stampfile = open( stampfname, O_CREAT|O_WRONLY|O_EXCL, 0600 );
        drop_groot();
        drop_root();

        if( stampfile < 0 ) {
            perror( _("Error: could not create stamp file in directory") );
            return -1;
        }
        close( stampfile );
    } else {
        /* exists, check that it is a directory */
        if( !S_ISDIR( st.st_mode ) ) {
            fprintf( stderr, _("Error: %s is not a directory\n"), dir );
            return -1;
        }
    }

    return 0;
}

int
assert_emptydir( const char* dirname )
{
    DIR* dir;
    struct dirent* dirent;

    /* we might need root for reading the dir */
    get_root();
    dir = opendir( dirname );
    drop_root();

    if( !dir ) {
        perror( _("Error: could not open directory") );
        return -1;
    }

    while ( ( dirent = readdir( dir ) ) ) {
        if( strcmp( dirent->d_name, "." ) && 
            strcmp( dirent->d_name, ".." ) &&
            strcmp( dirent->d_name, CREATED_DIR_STAMP ) ) {
            closedir( dir );
            fprintf( stderr, _("Error: directory %s is not empty\n"), dirname );
            return -1;
        }
    }

    closedir( dir );
    return 0;
}

int
is_dir( const char* path )
{
    struct stat st;

    if( stat( path, &st ) )
        return 0;

    return S_ISDIR( st.st_mode );
}

int
remove_pmount_mntpt( const char *path ) 
{
    char stampfile[PATH_MAX];
    int result = 0;

    result = snprintf( stampfile, sizeof( stampfile ), "%s/%s", path, CREATED_DIR_STAMP );
    if( result >= (int) sizeof( stampfile ) )
        return -1;

    get_root();
    if( !unlink( stampfile ) )
      result = rmdir( path );
    drop_root();
    return result;
}

unsigned
parse_unsigned( const char* s, int exitcode )
{
    char* endptr;
    long int result;

    /* return 0 on NULL or empty strings */
    if( !s || !*s )
        return 0;

    errno = 0;
    result = strtol( s, &endptr, 10 );
    if( *endptr == 0 && errno == 0 && result > 0 )
        return (unsigned) result;

    fprintf( stderr, _("Error: '%s' is not a valid number\n"), s );
    exit( exitcode );
}

int
pid_exists( unsigned pid )
{
    int result;

    get_root();
    result = kill( pid, 0 );
    drop_root();

    return (result == 0) ? 1 : 0;
}

int 
is_word_str( const char* s )
{
    const char* i;

    /* NULL or empty? */
    if( !s || !*s )
        return 0;

    for( i = s; *i; ++i ) {
        if( ( *i >= 'a' && *i <= 'z' ) ||
            ( *i >= 'A' && *i <= 'Z' ) ||
            ( *i >= '0' && *i <= '9' ) ||
            ( *i == '-' ) ||
            ( *i == '_' ) )
            continue;
        return 0;
    }

    return 1;
}

void
get_root()
{
    if( setreuid( -1, 0 ) ) {
        perror( _("Internal error: could not change to effective uid root") );
        exit( 100 );
    }
}

void
drop_root()
{
    if( setreuid( -1, getuid() ) ) {
        perror( _("Internal error: could not change effective user uid to real user id") );
        exit( 100 );
    }
}

void
get_groot()
{
    if( setregid( -1, 0 ) ) {
        perror( _("Internal error: could not change to effective gid root") );
        exit( 100 );
    }
}

void
drop_groot()
{
    if( setregid( -1, getgid() ) ) {
        perror( _("Internal error: could not change effective group id to real group id") );
        exit( 100 );
    }
}

