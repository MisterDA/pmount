/**
 * fs.h - data type and interface function for supported file systems
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 * (c) 2004 Canonical Ltd.
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#ifndef __fs_h
#define __fs_h

/**
 * Structure with information about a supported file system
 */
struct FS {
  const char* fsname;    /* file system name (e. g. 'ext2') */
  const char* options;   /* standard mount options (must not be empty) */
  int support_ugid;      /* whether the fs supports uid and gid options */
  const char* umask;     /* umask value (NULL if umask is not supported) */
  const char* iocharset_format;	 
  /* the format to be used for iocharset option -- NULL = iocharset=%s */
  const char* fdmask;    /* how to actually implement the fdmask format. 
			    Takes two unsigned ints as arguments: fmask then 
			    dmask */
  int skip_autodetect;   /* whether or not to skip this fs for detection */
};

/**
 * Return the information struct for a given file system, or NULL if the file
 * system is unknown. The returned pointer points to static data, do not free()
 * it.
 */
const struct FS* get_fs_info( const char* fsname );

/**
 * Return an array of supported file systems for iteration. The returned array
 * is terminated with an FS struct with fsname == NULL.
 */
const struct FS* get_supported_fs();

#endif /* !defined( __fs_h) */

