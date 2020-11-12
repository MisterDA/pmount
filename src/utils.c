/* -*- c-basic-offset: 4; -*- */
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

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <libintl.h>

#include <unistd.h>

#include "utils.h"

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
assert_dir( const char* dir, int create_stamp )
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

        if( create_stamp ) {
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
        }
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
is_block( const char* path )
{
    struct stat st;

    if( stat( path, &st ) )
        return 0;

    return S_ISBLK( st.st_mode );
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
    result = strtol( s, &endptr, 0 );
    if( *endptr == 0 && errno == 0 && result >= 0 )
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

int
spawnl( int options, const char* path, ... )
{
    char* argv[1024];
    unsigned argv_size = 0;
    va_list args;

    /* copy varargs to array */
    va_start( args, path );
    for(;;) {
        if( argv_size >= sizeof( argv ) ) {
            fprintf( stderr, "Internal error: spawnl(): too many arguments\n" );
            exit( 100 );
        }

        if( ( argv[argv_size++] = va_arg( args, char* ) ) == NULL )
            break;
    }
    va_end( args );

    return spawnv( options, path, argv );
}

char slurp_buffer[2048];

size_t slurp_size = 0;

#define DEVNULL_MASK (SPAWN_NO_STDOUT | SPAWN_NO_STDERR)
#define SLURP_MASK (SPAWN_SLURP_STDOUT | SPAWN_SLURP_STDERR)

int
spawnv( int options, const char* path, char *const argv[] )
{
    int devnull;
    int status;
    int i;
    pid_t new_pid;
    int fds[2];

    if( (options & SLURP_MASK) && pipe(fds) ) {
	perror(_("Impossible to setup pipes for subprocess communication"));
	exit( 100 );
    }

    if( enable_debug ) {
        printf( "spawnv(): executing %s", path );
        for( i = 0; argv[i]; ++i )
            printf( " '%s'", argv[i] );
        printf( "\n" );
    }

    new_pid = fork();
    if(new_pid == -1) {
	perror(_("Impossible to fork"));
	exit( 100 );
    }

    if( ! new_pid ) {
        if( options & SPAWN_EROOT )
            get_root();
        if( options & SPAWN_RROOT )
            if( setreuid( 0, -1 ) ) {
                perror( _("Error: could not raise to full root uid privileges") );
                exit( 100 );
            }

	/* Performing redirections */

	if( options & DEVNULL_MASK ) {
	    devnull = open( "/dev/null", O_WRONLY );
	    if( devnull > 0 ) {
		if( options & SPAWN_NO_STDOUT )
		    dup2( devnull, 1 );
		if( options & SPAWN_NO_STDERR )
		    dup2( devnull, 2 );
	    }
	    close( devnull );	/* Now useless */
	}
	if( options & SLURP_MASK ) {
	    close( fds[0] );	/* Close the read end of the pipe */

	    if( options & SPAWN_SLURP_STDOUT )
		dup2( fds[1], 1 );
	    if( options & SPAWN_SLURP_STDERR )
		dup2( fds[1], 2 );
	    close( fds[1] );	/* Now useless */
	}

        if( options & SPAWN_SEARCHPATH )
            execvp( path, argv );
        else
            execv( path, argv );
        exit( -1 );
    } else {

	/* First, slurp all data */
	if( options & SLURP_MASK ) {
	    close( fds[1] );	/* We don't need it */
	    int nb_read = 0;
	    slurp_size = 0;
	    do {
		nb_read = read(fds[0], slurp_buffer + slurp_size,
			    sizeof(slurp_buffer) - 1 - slurp_size);
	    	if(nb_read < 0) {
		    perror(_("Error while reading from child process"));
		    exit( 100 );
		}
		slurp_size += nb_read;
		if(slurp_size == sizeof(slurp_buffer) - 1)
		    break;
	    } while(nb_read);

	    if(nb_read) {
		fprintf(stderr, _("Child process output has exceeded buffer size, please file a bug report"));
	    }
	    close( fds[0] );	/* We close the reading end of the pipe */
	    slurp_buffer[slurp_size] = 0; /* Make it nul-terminated */
	}

        if( wait( &status ) < 0 ) {
            perror( "Error: could not wait for executed subprocess" );
            exit( 100 );
        }
    }

    if( !WIFEXITED( status ) ) {
        fprintf( stderr, "Internal error: spawn(): process did not return a status" );
        exit( 100 );
    }

    status = WEXITSTATUS( status );
    debug( "spawn(): %s terminated with status %i\n", path, status );
    return status;
}

/**
 * Internal function to determine lock file path for a given directory. The
 * returned string points to static memory and must not be free()'d.
 */
static
char*
get_dir_lockfile( const char* dir )
{
    static char name[PATH_MAX];
    static const char template[] = "/var/lock/pmount_%s";
    char *dir_fname;

    if( strlen( dir ) >= sizeof( name ) - sizeof( template ))
        return NULL;
    dir_fname = strreplace( dir, '/', '_' );
    snprintf( name, sizeof( name ), template, dir_fname );
    free (dir_fname );
    return name;
}

int
lock_dir( const char* dir ) {
    int f;
    char* lockfile = get_dir_lockfile( dir );
    if( !lockfile )
        return -1; /* name too long */

    get_root();
    f = creat( lockfile, 0600);
    drop_root();
    if (f < 0) {
        perror( "lock_dir(): creat" );
        return -1;
    }

    if( lockf( f, F_TLOCK, 0 ) == 0 )
        return 0;

    if (errno != EAGAIN)
        perror( "lock_dir(): lockf" );
    return -1;
}

void
unlock_dir( const char* dir ) {
    int f;
    char* lockfile = get_dir_lockfile( dir );
    if( !lockfile )
        return; /* name too long */

    get_root();
    f = open( lockfile, O_WRONLY);
    drop_root();
    if( f < 0 ) {
        if( errno != ENOENT )
            perror( "unlock_dir(): open" );
        return;
    }

    if( lockf( f, F_ULOCK, 0 ) != 0 )
        perror( "unlock_dir(): lockf" );

    get_root();
    unlink( lockfile );
    drop_root();
}
