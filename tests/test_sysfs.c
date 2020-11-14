/*
 * Copyright (c) 2009 Vincent Fourmond <fourmond@debian.org>
 *
 * This software is distributed under the terms and conditions of the
 * GNU General Public License. See file GPL for the full text of the license.
 */

/**
   This program tries uses find_sysfs_device from src/policy.c to find
   sysfs device information for the device given as argument.

   This program is only intended for development purposes.

   DO NOT INSTALL IT SUID ROOT !

   DO NOT RUN IT WITH ROOT PRIVILEGES ! (there's no need for that)
 */

#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* For stat: */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "policy.h"

int main(int argc, char *argv[])
{
  char *device_path = NULL;
  struct stat devstat;
  if(argc != 2) {
    fprintf(stderr, "Usage: %s device\n", argv[0]);
    return EXIT_FAILURE;
  }
  if(stat(argv[1], &devstat)) {
    fprintf(stderr, "stat(%s): %s\n", argv[1], strerror(errno));
    return EXIT_FAILURE;
  }
  if(! (devstat.st_mode & S_IFBLK)) {
    fprintf(stderr, "Sorry, `%s' is not a block device.\n", argv[1]);
    return EXIT_FAILURE;
  }

  if(find_sysfs_device(argv[1], &device_path)) {
    const char * bus;
    fprintf(stdout, "Found sysfs device for %s: %s\n", argv[1], device_path);
    fprintf(stdout, "Device %s is removable: %s\n", argv[1],
            is_blockdev_attr_true(device_path,"removable") ? "yes" :"no");
    bus = bus_has_ancestry(device_path, hotplug_buses);
    if(bus)
     fprintf(stdout, "Found allowlisted bus: %s\n", bus);
    else
      fprintf(stdout, "No allowlisted bus found\n");
    free(device_path);
    return EXIT_SUCCESS;
  } else {
    fprintf(stderr, "find_sysfs_device failed for %s\n", argv[1]);
    return EXIT_FAILURE;
  }
}
