
#ifndef CONF_H
#define	CONF_H

#define CONF_FILE               "/etc/pmount.conf"


int
get_conf_for_device(const char *device, char **fs, char **charset,
        char **passphrase, char **mntpt, char **options);

void
conf_set_value( char *buf, char **dest );


#endif	/* CONF_H */

