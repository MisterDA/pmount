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

/**********************************************************************/
/* Configuration items  */

/**
   Whether or not the user is allowed to run fsck or not.
*/
static int configuration_allow_fsck = 0;

int conffile_allow_fsck()
{
  return configuration_allow_fsck;
}




/**********************************************************************/
/* Helper functions for parsing the configuration file. */

/**
   Reads a line of configuration file into the given target buffer.

   Though for now it isn't the case, it will eventually handle:
   * removing the last \n character (a la chomp)
   * escaping the end-of-line with a \
*/
static int conffile_read_line(FILE * file, char * dest, size_t nb)
{
  int len;
  if( ! fgets(dest, nb, file) ) {
    perror(_("Failed to read configuration file"));
    return -1;
  }
  /* Here, we should perform backslash escape, and basic checking that
     the line isn't too long */
  len = strlen(dest);
  if(dest[len-1] != '\n' && ! feof(file)) {
    fprintf(stderr, _("Line too long in configuration file: %s\n"),
	    dest);
    return -1;
  }
  return 0;
}

/**
   Patterns for parsing the configuration files.
*/
static int regex_compiled = 0;
regex_t comment_RE, declaration_RE, uint_RE, blank_RE, true_RE, false_RE;   

/**
   Initialize all the patterns necessary for parsing the configuration
   file.
*/
static int conffile_prepare_regexps()
{
  /* A regexp matching comment lines */
  if( regcomp(&comment_RE, "^[[:blank:]]*#", REG_EXTENDED)) {
    perror(_("Could not compile regular expression for comments"));
    return -1;
  }
  /* A regexp matching a boolean value*/

  if( regcomp(&declaration_RE, 
	      "^[[:blank:]]*([a-zA-Z_]+)[[:blank:]]*"
	      "=[[:blank:]]*(.*)$",
	      REG_EXTENDED )) {
    perror(_("Could not compile regular expression for boolean values"));
    return -1;
  }

  if( regcomp(&true_RE, "^[[:blank:]]*(true|yes|on)[[:blank:]]*", 
	      REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
    perror(_("Could not compile regular expression for true values"));
    return -1;
  }

  if( regcomp(&false_RE, "^[[:blank:]]*(false|no|off)[[:blank:]]*", 
	      REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
    perror(_("Could not compile regular expression for false values"));
    return -1;
  }

  if( regcomp(&uint_RE, 
	      "^[[:blank:]]*([a-zA-Z_]+)[[:blank:]]*"
	      "=[[:blank:]]*([0-9]+)$",
	      REG_EXTENDED )) {
    perror(_("Could not compile regular expression for integer values"));
    return -1;
  }

  if( regcomp(&blank_RE, "^[[:blank:]]*\n$", REG_EXTENDED )) {
    perror(_("Could not compile regular expression for blank lines"));
    return -1;
  }

  return 0;
}

/**
   Frees the pattern space of the allocated regular expressions
*/
static void conffile_free_regexps()
{
  regfree(&comment_RE);
  regfree(&uint_RE);
  regfree(&declaration_RE);
  regfree(&blank_RE);
  regfree(&true_RE);
  regfree(&false_RE);
}


#define BLANK_LINE 0
#define DECLARATION_LINE 1


/**
   Classifies the given line into several categories:

   * blank or comment (0)
   * a declaration
   * beginning of a FS specification ? (later on)

   When applicable, this function puts the adress of the relevant bits
   into the pointer variables.

   Returns -1 when failed.
 */
static int conffile_classify_line(char * line, char ** name_ptr,
				  char ** value_ptr)
{
  regmatch_t m[3];
  if( ! regexec( &comment_RE, line, 0, NULL, 0) ||
      ! regexec( &blank_RE, line, 0, NULL, 0))
    return BLANK_LINE;

  if( ! regexec( &declaration_RE, line, 3, m, 0) ) {
    /* OK, we found a line containing something */
    if(m[1].rm_so < 0) {
      fprintf(stderr, "There sure was a problem, it is in principle impossible that this happens\n");
      return -1;
    }
    *name_ptr = line + m[1].rm_so;
    *(line + m[1].rm_eo) = 0;	/* Make it NULL-terminated */

    if(m[2].rm_so < 0) {
      fprintf(stderr, "There sure was a problem, it is in principle impossible that this happens\n");
      return -1;
    }
    *value_ptr = line + m[2].rm_so;
    *(line + m[2].rm_eo) = 0;	/* Make it NULL-terminated */
    if(*(line + m[2].rm_eo - 1) == '\n')
      *(line + m[2].rm_eo - 1) = 0; /* Strip trailing newline when
				       applicable */    
    return DECLARATION_LINE;
  }
  return -1;			/* Should never be reached. */
}

/**
   Checks that the given value is a boolean and store is value in
   target.
 */
int conffile_get_boolean(const char * value, int * target)
{
  if( ! regexec( &true_RE, value, 0, NULL, 0))
    *target = 1;
  else if( ! regexec( &false_RE, value, 0, NULL, 0))
    *target = 0;
  else {
    fprintf(stderr, _("Error while reading configuration file: '%s' "
		      "is not a boolean value"), value);
    return -1;
  }
  return 0;
}


int conffile_read(const char * file)
{
  /** @todo for now, no cleanup is performed on error...*/
  char line_buffer[1000];
  char * name;
  char * value;
  FILE * f;

  /* Compile regular expressions when necessary */
  conffile_prepare_regexps();


  f = fopen( file, "r" );
  if(! f) {
    perror(_("Failed to open configuration file"));
    return -2;
  }
  fprintf(stdout, "Reading file: %s\n", file);

  while(! feof(f)) {
    if(conffile_read_line(f, line_buffer, sizeof(line_buffer)))
      return -1;
    int line_type = conffile_classify_line(line_buffer, &name, &value);
    switch(line_type) {
    case BLANK_LINE:
      break;
    case DECLARATION_LINE:
      /* Now, another switch-like  */
      fprintf(stderr, "Name: '%s' -- value: '%s'\n", name, value);
      if(! strcmp(name, "allow_fsck")) {
	if(conffile_get_boolean(value, &configuration_allow_fsck))
	  return -1;
      }
      else {
	fprintf(stderr, _("Error parsing configuration file: "
			  "unknown field '%s'\n"),
		name);
	return -1;
      }
      break;
    default:
      fprintf(stderr, "Error parsing configuration file line: %s\n", 
	      line_buffer);
      return -1;
    }
  }
  fclose(f);

  return 0;
}

int conffile_system_read()
{
  struct stat st;
  /* If the system configuration file does not exist, we don't
     complain... */
  if( stat( SYSTEM_CONFFILE, &st) )
    return 0;
  return 0;
}
