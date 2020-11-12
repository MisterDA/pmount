/**
 * loop.c -- handling loop devices
 *
 * Author: Vincent Fourmond <fourmond@debian.org>
 *         Copyright 2011 by Vincent Fourmond
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* We unfortunately need regular expressions... */
#include <regex.h>

#include <libintl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


#include "configuration.h"
#include "utils.h"

/* Hmmm, I'll need backquotes here... A fun thing to do safely... */


/**
   Tries all whitelisted loop devices to find one which isn't used,
   and returns it.

   Returns NULL if no device could be found.
 */
static const char * loopdev_find_unused()
{
  char ** devices = conffile_loop_devices();
  if(! devices)
    return NULL;

  while(*devices) {
    if(strlen(*devices) > 0) {
      debug("Trying loop device: %s\n", *devices);
      int result = spawnl(SPAWN_EROOT | SPAWN_NO_STDOUT | SPAWN_NO_STDERR,
			  LOSETUPPROG, LOSETUPPROG, *devices, NULL);
      if(result == 1)		/* Device is not configured, see losetup(8) */
	return *devices;
    }
    devices++;
  }
}

int loopdev_is_whitelisted(const char * device)
{
  char ** devices = conffile_loop_devices();
  if(! devices)
    return 0;

  while(*devices) {
    if(! strcmp(*devices, device))
      return 1;
    devices++;
  }
  return 0;
}

int loopdev_dissociate(const char * device)
{
  int result = 1 ;
  int nb_tries = 0;
  while(result && nb_tries < 10) {
    result = spawnl(SPAWN_EROOT, LOSETUPPROG, LOSETUPPROG,
		    "-d", device, NULL);
    if(result) {
      debug("The loop device may be busy, trying again to dissociate\n");
      sleep(1);
    }
  }
  return result ? -1 : 0;
}


int loopdev_associate(const char * source, char * target, size_t size)
{
  struct stat before;
  const char * device;
  char buffer[1024];
  int result;
  int fd;

  fd = open(source, O_RDWR);
  if( fd == -1 ) {
    snprintf(buffer, sizeof(buffer),
	     _("Failed to open file '%s' for reading"),
	     source);
    perror(buffer);
    return -1;
  }

  /**
     First, stat the file and check the permissions:
     owner + read/write

     @todo Maybe the simple fact that the above open will fail if
     the user does not have read/write permissions is enough ?

  */
  if(fstat(fd, &before)) {
    snprintf(buffer, sizeof(buffer),
	     _("Failed to stat file '%s'"),
	     source);
    perror(buffer);
    close(fd);
    return -1;
  }

  if(! (before.st_uid == getuid() &&
	(before.st_mode & S_IRUSR) && /* readable */
	(before.st_mode & S_IWUSR)    /* writable */
	)) {
    fprintf(stderr, _("For loop mounting, you must be the owner of %s and "
		      "have read-write permissions on it\n"), source);
    close(fd);
    return -1;
  }

  device = loopdev_find_unused();
  if(! device) {
    fprintf(stderr, _("No whitelisted loop device available\n"));
    close(fd);
    return -1;
  }
  debug("Found an unused loop device: %s\n", device);


  /* We use /dev/fd/... to ensure that the file used is the statted
     one  */
  snprintf(buffer, sizeof(buffer), "/dev/fd/%d", fd);

  result = spawnl(SPAWN_EROOT, LOSETUPPROG, LOSETUPPROG,
		  device, buffer, NULL);
  close(fd); 			/* Now useless */

  if(result) {
    fprintf(stderr, _("Failed to setup loopback device\n"));
    return -1;
  }


  /* Copy the device to the target */
  snprintf(target, size, "%s", device);
  return 0;			/* Everything went fine ! */

}
