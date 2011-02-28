/**
 * conffile.c -- parsing of the configuration file
 *
 * Author: Vincent Fourmond <fourmond@debian.org>
 *         (c) 2009, 2011 by Vincent Fourmond
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


/**
   First, we have a whole set of configuration items, that is objects
   that represent a "value" of the configuration file.
*/

void ci_bool_set_default(ci_bool * c, int val) {
  c->def = val;
}

int ci_bool_allowed(ci_bool * c) {
  return c->def;
}


/**************************************************/

/**
   Returns the number of keys for a given cf_spec.
*/
static size_t cf_spec_key_number(const cf_spec * spec)
{
  switch(spec->type) {
  case boolean_item:
    return 1;			/* But this will change */
  default:
    return 0;
  };
}

/**
   A structure used to associate a configuration key to the target
   configuration item
*/
typedef struct {
  /**
     The key
  */
  char * key;
  
  /**
     The target
  */
  cf_spec * target;

  /**
     Additional info, in the form of a void pointer
  */
  void * info;
} cf_key;

/**
   Prepares the pairs key/target for the given spec into the area
   pointed to by pairs. Enough space must have been reserved
   beforehand.
*/
static void cf_spec_prepare_keys(cf_spec * spec, cf_key * keys)
{
  /* I guess this will be common to everything. */
  int l,l2;
  switch(spec->type) {
  case boolean_item:
    /* We create a "base"_allow key*/
    l = strlen(spec->base);
    l2 = l + strlen("_allow") +1;
    keys->key = malloc(l2);
    snprintf(keys->key,l2, "%s_allow", spec->base);
    keys->target = spec;
    keys->info = NULL;
    /* Then, we could create "base"_allow_user, "base"_allow_group and
       "base"_deny_user 
    */
    return;
  default:
    return;
  };
}

/**
   Takes a "null"-terminated list of cf_spec objects and returns a
   newly allocated cf_pair array.
*/
static cf_key * cf_spec_build_keys(cf_spec * specs) 
{
  cf_spec * s = specs;
  cf_key * keys;
  cf_key * k;
  size_t nb = 0;
  while(s->base) {
    nb += cf_spec_key_number(s);
    s++;
  }
  
  keys = malloc(sizeof(cf_key) * (nb+1));
  k = keys;
  s = specs;
  while(s->base) {
    cf_spec_prepare_keys(s, keys);
    keys += cf_spec_key_number(s);
    s++;
  }
  keys->key = NULL;
  return k;
}

/**
   @todo write some clean up code
*/
static void cf_key_free_keys(cf_key * keys)
{
}


/**
   Finds withing the given pairs the cf_spec corresponding to the
   given key 
*/
static cf_key * cf_key_find(const char * key, cf_key * keys)
{
  while(keys->key) {
    if(! strcmp(key, keys->key))
      return keys;
    keys++;
  }
  return NULL;
}


/**
   Reads a line of configuration file into the given target buffer.

   Though for now it isn't the case, it will eventually handle:
   * escaping the end-of-line with a \
*/
static int cf_read_line(FILE * file, char * dest, size_t nb)
{
  int len;
  if( ! fgets(dest, nb, file)) {
    if(feof(file)) {
      *dest = 0;
      return 0;
    }
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
static regex_t comment_RE, declaration_RE, uint_RE, 
  blank_RE, true_RE, false_RE;   

/**
   Initialize all the patterns necessary for parsing the configuration
   file.
*/
static int cf_prepare_regexps()
{
  /* A regexp matching comment lines */
  if( regcomp(&comment_RE, "^[[:blank:]]*#", REG_EXTENDED)) {
    perror(_("Could not compile regular expression for comments"));
    return -1;
  }
  /* A regexp matching a boolean value*/

  if( regcomp(&declaration_RE, 
	      "^[[:blank:]]*([-a-zA-Z_]+)[[:blank:]]*"
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
static void cf_free_regexps()
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
   into the pointer variables; in that case, it modifies the buffer
   in-place.

   Returns -1 when failed.
 */
static int cf_classify_line(char * line, char ** name_ptr,
				  char ** value_ptr)
{
  regmatch_t m[3];
  if(! (*line) ||
     ! regexec( &comment_RE, line, 0, NULL, 0) ||
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
int cf_get_boolean(const char * value, int * target)
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

/**
   Assigns the value to the given key, ensuring that the value match.
*/
static int cf_key_assign_value(cf_key * key, const char * value)
{
  switch(key->target->type) {
    int val;
  case boolean_item:
    /* Implement groups/users... */
    if(! cf_get_boolean(value, &val)) {
      ci_bool_set_default((ci_bool *)key->target->target, val);
      return 0;
    }
    return -1;
  default:
    break;
  }
}


int cf_read_file(FILE * file, cf_spec * specs)
{
  char line_buffer[1000];
  char * name;
  char * value;
  cf_key * keys = cf_spec_build_keys(specs);

  /* Compile regular expressions when necessary */
  if(cf_prepare_regexps()) 
    return -1;


  while(! feof(file)) {
    int line_type;
    cf_key * key;
    if(cf_read_line(file, line_buffer, sizeof(line_buffer)))
      return -1;
    line_type = cf_classify_line(line_buffer, &name, &value);
    switch(line_type) {
    case BLANK_LINE:
      break;
    case DECLARATION_LINE:
      key = cf_key_find(name, keys);
      if(key) {
	if(cf_key_assign_value(key, value)) 
	  return -2;
      }
      else {
	fprintf(stderr, "Error: key '%s' is unknown\n", name);
      }
      break;
    default:
      fprintf(stderr, "Error parsing configuration file line: %s\n", 
	      line_buffer);
      return -1;
    }
  }
  return 0;
}
