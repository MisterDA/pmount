/**
 * @file fs.h - data type and interface function for supported file systems
 *
 * Author: Martin Pitt <martin.pitt@canonical.com>
 *         (c) 2004 Canonical Ltd.
 *         Vincent Fourmond <fourmond@debian.org>
 *         (c) 2007-2009 by Vincent Fourmond
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
  /** File system name (e. g. 'ext2') */
  const char* fsname;
  /** Standard mount options (must not be empty) */
  const char* options;   
  /** Whether the fs supports uid and gid options */
  int support_ugid;      
  /** umask value (NULL if umask is not supported) */
  const char* umask;     
  /** The printf-like format to be used for iocharset option -- NULL means
      iocharset=%s */
  const char* iocharset_format;	 
  /** A printf-like format for actually implementing the fdmaskx.  If
      not null, the format should take two unsigned ints as arguments:
      fmask then dmask */
  const char* fdmask;    
  /** Whether or not to skip this fs for detection */
  int skip_autodetect;   
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

