/**
 * loop.c -- handling loop devices
 *
 * Author: Vincent Fourmond <fourmond@debian.org>
 *         Copyright 2011 by Vincent Fourmond
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* We unfortunately need regular expressions... */
#include <regex.h>

#include <libintl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include "configuration.h"
#include "config.h"
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
  struct stat before, after;
  const char * device;
  int result;
  
  /* First, stat the file and check the permissions:
     owner + read/write*/
  if(stat(source, &before)) {
    perror(_("Failed to stat file %s"));
    return -1;
  }
    
  if(! (before.st_uid == getuid() && 
	(before.st_mode & S_IRUSR) && /* readable */
	(before.st_mode & S_IWUSR)    /* writable */
	)) {
    /**
       @todo Maybe this check will have to evolve some day to
       supporting read-only mounts and files with a different owner
       (but maybe not for read-write mount ?)
     */
    fprintf(stderr, _("For loop mounting, you must be the owner of %s and "
		      "have read-write permissions on it\n"), source);
    return -1;
  }
  device = loopdev_find_unused();
  if(! device) {
    fprintf(stderr, _("No whitelisted loop device available\n"));
    return -1;
  }
  debug("Found an unused loop device: %s\n", device);
  
  /* This code is vulnerable to a race condition: it is possible for
     the loop device to have been used up before running losetup. But
     that doesn't cause any security problems.
  */

  result = spawnl(SPAWN_EROOT, LOSETUPPROG, LOSETUPPROG, 
		  device, source, NULL);
  if(result) {
    fprintf(stderr, _("Failed to setup loopback device\n"));
    return -1;
  }

  /* Now, we need to check that the file didn't change... */
  result = stat(source, &after);
  if(result ||
     (after.st_dev != before.st_dev) ||
     (after.st_dev != before.st_dev) ||
     (after.st_mode != before.st_mode) ||
     (after.st_uid != before.st_uid)) {
    fprintf(stderr, _("File %s changed during the call to losetup, aborting\n"),
	    source);
    /*  */
    result = loopdev_dissociate(device);
    /* Hmmm... I don't know what to do in case losetup -d fails*/
    return -1;
  }

  /* The last thing we should do is to check using losetup that the
     dev/inode numbers of the file the loop is associated to is
     correct, else it is still possible for an attacker to swap the
     file just before the call to losetup and to switch it back just
     after.

     This however requires "backquotes", so it will have to wait.
   */

  if(0)
    ;

  /* Copy the device to the target */
  snprintf(target, size, "%s", device);
  return 0;			/* Everything went fine ! */

}
