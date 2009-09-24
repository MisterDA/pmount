/**
 * conffile.c -- parsing of the configuration file
 *
 * Author: Vincent Fourmond <fourmond@debian.org>
 *         (c) 2009 by Vincent Fourmond
 * 
 * This software is distributed under the terms and conditions of the 
 * GNU General Public License. See file GPL for the full text of the license.
 */

#include <stdio.h>
#include <string.h>
/* For regular expression parsing... */
#include <regex.h>

#include <libintl.h>
#include <sys/stat.h>

#include "conffile.h"
#include "config.h"
#include "utils.h"

ConfFile system_configuration = { .allow_fsck = 0 };

void conffile_init(ConfFile * cf)
{
  memset(cf, 0, sizeof(ConfFile));
}


int conffile_read(const char * file, ConfFile * cf)
{
  regex_t comment_RE, boolean_RE, blank_RE, true_RE, false_RE;   
  char line_buffer[200];
  char buffer[200];
  FILE * f;
  regmatch_t m[3];
  /** @todo free regular expressions on error - must use gotos ? */
  if( regcomp(&comment_RE, "^[[:blank:]]*#", REG_EXTENDED)) {
    perror(_("Could not compile regular expression for comments"));
    return -1;
  }

  if( regcomp(&boolean_RE, 
	      "^[[:blank:]]*([a-zA-Z_]+)[[:blank:]]*"
	      "=[[:blank:]]*(.*)$",
	      REG_EXTENDED )) {
    perror(_("Could not compile regular expression for values"));
    return -1;
  }

  if( regcomp(&blank_RE, "^[[:blank:]]*\n$", REG_EXTENDED )) {
    perror(_("Could not compile regular expression for blank lines"));
    return -1;
  }

  if( regcomp(&true_RE, "[[:blank:]]*(true|yes)[[:blank:]]*", 
	      REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
    perror(_("Could not compile regular expression for true values"));
    return -1;
  }

  if( regcomp(&false_RE, "[[:blank:]]*(false|no)[[:blank:]]*", 
	      REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
    perror(_("Could not compile regular expression for false values"));
    return -1;
  }

  f = fopen( file, "r" );
  if(! f) {
    perror(_("Failed to open configuration file"));
    return -2;
  }

  while(! feof(f)) {
    if( ! fgets(line_buffer, sizeof(line_buffer), f) ) {
      perror(_("Failed to read configuration file"));
      fclose(f);
      return -3;
    }
    /** @todo check that lines are not too long...*/
    if( ! regexec( &comment_RE, line_buffer, 0, NULL, 0) ||
	! regexec( &blank_RE, line_buffer, 0, NULL, 0)
	) {
      /* Comment/blank */
      continue;
    }

    /** @todo if there is a need for non-boolean data, insert a check
	with a validating regexp 
    */

    if( ! regexec( &boolean_RE, line_buffer, 3, m, 0) ) {
      /* Value line */
      int value;
      /* We use memcpy as strncpy does not add null when necessary */
      memcpy(buffer, line_buffer + m[2].rm_so, m[2].rm_eo - m[2].rm_so);
      buffer[m[2].rm_eo - m[2].rm_so] = 0;
      if( ! regexec( &false_RE, buffer, 0, NULL, 0) ) {
	value = 0;
      }
      else if( ! regexec( &true_RE, buffer, 0, NULL, 0) ) {
	value = 1;
      }
      else {
	fprintf(stderr, "Value %s is not a boolean\n", buffer);
	return -4;
      }

      /* Now, checking the name of the feature */
      memcpy(buffer, line_buffer + m[1].rm_so, m[1].rm_eo - m[1].rm_so);
      buffer[m[1].rm_eo - m[1].rm_so] = 0;
      if(! strcmp(buffer, "allow_fsck")) {
	cf->allow_fsck = value;
      }
      else {
	fprintf(stderr, "Invalid configuration item: '%s'\n", buffer);
	return -4;
      }
      
    }
    else {
      fprintf(stderr, "Invalid line in configuration file %s:\n\t'%s'\n",
	      file, line_buffer);
      return -4;
    }
      
    
  }
  fclose(f);
  regfree(&comment_RE);
  regfree(&boolean_RE);
  regfree(&blank_RE);
  regfree(&true_RE);
  regfree(&false_RE);

  return 0;
}

int conffile_system_read()
{
  struct stat st;
  /* First set to defaults */
  conffile_init(&system_configuration);
  
  /* If the system configuration file does not exist, we don't
     complain... */
  if( stat( SYSTEM_CONFFILE, &st) )
    return 0;
  return conffile_read( SYSTEM_CONFFILE, &system_configuration);
}
