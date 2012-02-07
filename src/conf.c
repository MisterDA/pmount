
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "conf.h"
#include "utils.h"
#include "realpath.h"

#define BUFLEN                  1024

int
get_conf_for_device(const char *device, char **fs, char **charset,
        char **passphrase, char **mntpt, char **options)
{
    FILE *f;
    char buffer[BUFLEN], section[PATH_MAX];
    char *buf, *sec, *s;
    int skip_section = 0;
    int in_section = 0;
    int len;

    if( NULL == ( f = fopen( CONF_FILE, "r" ) ) ) {
        debug( "unable to open conf file %s\n", CONF_FILE );
        return 1;
    }
    sec = NULL;
    while( !feof( f ) ) {
        if( fgets( buffer, BUFLEN, f ) ) {
            /* skip spaces & tabs */
            for ( buf = buffer; *buf == ' ' || *buf == '\t'; ++buf )
                ;
            /* ignore commented & empty lines */
            if( *buf == ';' || *buf == '#' || *buf == '\n' ) {
                continue;
            }
            /* new section? */
            if( *buf == '[' ) {
                /* if we were in section, we're done */
                if( in_section ) {
                    break;
                }
                ++buf;
                if( NULL == ( s = strchr( buf, ']' ) ) ) {
                    fclose( f );
                    fprintf( stderr, "invalid syntax in %s: %s\n", CONF_FILE, buf );
                    return 2;
                }
                if( s - buf >= PATH_MAX ) {
                    fclose( f );
                    fprintf( stderr, "invalid section name in %s: %s\n", CONF_FILE, buf );
                    return 3;
                }
                strncpy( section, buf, s - buf );
                /* NULL-terminate the string */
                s = section + (s - buf);
                *s = 0;
                debug( "found section for %s\n", section );
                /* try to resolve, might be e.g. a /dev/disk/by-uuid/... */
                if( !realpath( section, section ) ) {
                    if( !is_block( section ) ) {
                        /* probably section for a device not plugged in */
                        debug( "unable to resolve, not a block, skipping section\n" );
                        skip_section = 1;
                        continue;
                    }
                } else {
                    debug( "resolved to %s\n", section );
                }
                /* is this the device we're looking for? */
                if( strcmp( device, section ) ) {
                    debug( "no match, skipping section\n" );
                    skip_section = 1;
                    continue;
                }
                debug( "match found!\n" );
                sec = section;
                skip_section = 0;
                in_section = 1;
                continue;
            } else if( skip_section ) {
                continue;
            } else if( NULL == sec ) {
                fclose( f );
                debug( "no matching section found\n" );
                return -1;
            }
            /* we're in a section, must be a name=value */
            if ( NULL == ( s = strchr( buf, '=' ) ) ) {
                fprintf( stderr, "invalid syntax in %s: %s\n", CONF_FILE, buf );
                continue;
            }
            /* ignore spaces & tabs */
            for ( --s; *s == ' ' || *s == '\t'; --s )
                ;
            ++s;
            len = s - buf;
            if( NULL != fs && !strncmp( buf, "fs", len ) ) {
                conf_set_value( buf, fs );
                debug( "file system set to %s\n", *fs );
            } else if( NULL != charset && !strncmp( buf, "charset", len )) {
                conf_set_value( buf, charset );
                debug( "charset set to %s\n", *charset );
            } else if( NULL != passphrase && !strncmp( buf, "passphrase", len )) {
                conf_set_value( buf, passphrase );
                debug( "passphrase set to %s\n", *passphrase );
            } else if( NULL != mntpt && !strncmp( buf, "mntpt", len )) {
                conf_set_value( buf, mntpt );
                debug( "mount point set to %s\n", *mntpt );
            } else if( NULL != options && !strncmp( buf, "options", len )) {
                conf_set_value( buf, options );
                debug( "options set to %s\n", *options );
            } else {
                debug( "ignoring: %s", buf );
            }
        }
    }
    fclose( f );
    return !in_section;
}

void
conf_set_value( char *buf, char **dest )
{
    char *s;
    /* position to beginning of value */
    buf = strchr( buf, '=' );
    /* skip spaces & tabs */
    for( ++buf; *buf == ' ' || *buf == '\t'; ++buf)
        ;
    /* find end position */
    for( s = buf; *s != ' ' && *s != ';' && *s != '#' && *s != '\n' && *s != 0; ++s)
        ;
    /* set value */
    *dest = strndup( buf, s - buf );
}
