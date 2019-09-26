#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>

#include "utils.h"
#include "configuration.h"
#include "shared.h"

void
shared_init(void)
{
    int result;

    /* initialize locale */
    setlocale( LC_ALL, "" );
    bindtextdomain( "pmount", NULL );
    textdomain( "pmount" );

    /* are we root? */
    if( geteuid() ) {
        fputs( _("Error: this program needs to be installed suid root\n"),
               stderr );
        exit(E_INTERNAL);
    }

    /* drop root privileges until we really need them (still available
       as saved uid) */
    result = seteuid( getuid() );
    if( result == -1 ) {
        perror("seteuid");
        exit(E_INTERNAL);
    }

    if( conffile_system_read() ) {
        fputs( _("Error while reading system configuration file\n"), stderr );
        exit(E_INTERNAL);
    }
}

char *
device_realpath(const char *path, int *is_real_path)
{
    char *device = NULL;

    device = realpath( path, NULL );
    if( device ) {
        debug( "resolved %s to device %s\n", path, device );
        *is_real_path = 1;
    } else {
        *is_real_path = 0;
        debug( "%s cannot be resolved to a proper device node\n", path );
        device = strdup( path );
        if( !device ) {
            perror("strdup");
            exit(E_INTERNAL);
        }
    }
    return device;
}
