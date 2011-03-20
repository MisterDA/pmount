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
#include <stdlib.h>
#include <string.h>
/* For regular expression parsing... */
#include <regex.h>

#include <libintl.h>
#include <sys/stat.h>


/* For getting uids... */
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "conffile.h"
#include "config.h"
#include "utils.h"


/**
   Whether the given uid is in the -1-terminated list
 */
static int cf_uid_in_list(uid_t value, uid_t * list)
{
  if(! list)
    return 0;
  while(*list != -1) {
    if(*list == value)
      return 1;
    else
      list++;
  }
  return 0;
}

/**
   The groups the user belongs to. We use a cache to avoid querying
   too often.
*/
static gid_t * user_groups = NULL;

static int cf_get_groups()
{
  int nb;
  if(user_groups)
    return 0;			/* Everything fine */
  nb = getgroups(0, NULL);
  if(nb < 0)
    return -1;
  user_groups = malloc((nb + 1) * sizeof(gid_t));
  nb = getgroups(nb, user_groups);
  if(nb < 0)
    return -1;
  user_groups[nb] = -1;		/* -1 -terminated string */
  return 0;
}


/**
   Whether the given group belongs to the user's groups.
 */
static int cf_gid_within_groups(gid_t gid)
{
  gid_t * grp;
  if(cf_get_groups() < 0) {
    perror("Failed to get group information");
    exit(1);			/* Violent, but, well... */
  }
  grp = user_groups;
  while(*grp != -1) {
    if(*grp == gid)
      return 1;
    grp++;
  } 
  return 0;
}

/**
   Whether the user's group contain at least one of those listed in
   the -1 -terminated list of gids.

   Yes, I know that this function is inefficient, and that sorting
   both lists beforehand would do miracles. On the other hand, unless
   both group lists are huge, the time spent here isn't very large.
 */
static int cf_user_has_groups(gid_t * gid_list)
{
  if(! gid_list)
    return 0;
  while(*gid_list != -1) {
    if(cf_gid_within_groups(*gid_list))
      return 1;
    gid_list++;
  }
  return 0;
}




/**
   First, we have a whole set of configuration items, that is objects
   that represent a "value" of the configuration file.
*/

void ci_bool_set_default(ci_bool * c, int val) 
{
  c->def = val;
}

int ci_bool_allowed(ci_bool * c) 
{
  if(c->def) {
    /* Allowed by default, we just check the uid isn't in the
       denied user list */
    if(cf_uid_in_list(getuid(), c->denied_users))
      return 0;			/* Denied.. */
    return 1;
  }
  else {
    if(cf_uid_in_list(getuid(), c->allowed_users))
      return 1;
    if(cf_user_has_groups(c->allowed_groups))
      return 1;
    return 0;
  }
}

void ci_bool_dump(ci_bool * c, FILE * out)
{
  fprintf(out, "Default: %s\n", (c->def ? "allowed" : "denied"));
  if(c->allowed_groups) {
    fprintf(out, "Allowed groups:");
    gid_t * g = c->allowed_groups;
    while(*g != -1) {
      fprintf(out, " %d", *g);
      g++;
    }
    fprintf(out, "\n");
  }

  if(c->allowed_users) {
    fprintf(out, "Allowed users:");
    uid_t * g = c->allowed_users;
    while(*g != -1) {
      fprintf(out, " %d", *g);
      g++;
    }
    fprintf(out, "\n");
  }

  if(c->denied_users) {
    fprintf(out, "Denied users:");
    uid_t * g = c->denied_users;
    while(*g != -1) {
      fprintf(out, " %d", *g);
      g++;
    }
    fprintf(out, "\n");
  }
  fprintf(out, "-> result: %s\n", (ci_bool_allowed(c) ? "allowed" : "denied"));
}

/**************************************************/

/**
   Returns the number of keys for a given cf_spec.
*/
static size_t cf_spec_key_number(const cf_spec * spec)
{
  switch(spec->type) {
  case boolean_item:
    return 4;
  case string_list:
    return 1;
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
    keys++;

    l2 = l + strlen("_allow_user") +1;
    keys->key = malloc(l2);
    snprintf(keys->key,l2, "%s_allow_user", spec->base);
    keys->target = spec;
    keys->info = (void*)1L;
    keys++;

    l2 = l + strlen("_allow_group") +1;
    keys->key = malloc(l2);
    snprintf(keys->key,l2, "%s_allow_group", spec->base);
    keys->target = spec;
    keys->info = (void*)2L;
    keys++;

    l2 = l + strlen("_deny_user") +1;
    keys->key = malloc(l2);
    snprintf(keys->key,l2, "%s_deny_user", spec->base);
    keys->target = spec;
    keys->info = (void*)3L;
    return;
  case string_list:
    keys->key = strdup(spec->base);
    keys->target = spec;
    keys->info = NULL;
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
   Free the key structure.
*/
static void cf_key_free_keys(cf_key * keys)
{
  cf_key * k = keys;
  while(k->key) {
    free(k->key);
    k++;
  }
  free(keys);
}


/**
   Finds withing the given pairs the cf_spec corresponding to the
   given key.

   Yes, it's inefficient, but do we really need more efficient than
   this ?
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

*/
static int cf_read_line(FILE * file, char * dest, size_t nb)
{
  int len;
  while(1) {
    if( ! fgets(dest, nb, file)) {
      if(feof(file)) {
	*dest = 0;
	return 0;
      }
      perror(_("Failed to read configuration file"));
      return -1;
    }

    len = strlen(dest);
    if(dest[len-1] != '\n' && ! feof(file)) {
      fprintf(stderr, _("Line too long in configuration file: %s\n"),
	      dest);
      return -1;
    }
    if(len < 2 || dest[len-2] != '\\')
      return 0;
    
    /* Multi-line, we go on. */
    dest += (len - 2);
    nb -= (len - 2);
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
static int cf_get_boolean(const char * value, int * target)
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
   Returns the number of characters within the which set in str
*/
static size_t cf_count_chars(const char * str, const char * which)
{
  size_t val = 0;
  while(1) {
    str = strpbrk(str, which);
    if(! str)
      break;
    str++;
    val++;
  }
  return val;
}

/**
   Trim white space while copying from one string to another.
*/
static void cf_trim_anew(const char * source, char * dest, size_t nb)
{
  size_t l = strspn(source, " \t\n");
  source += l;
  /* this is like strncpy, but without the security problems */
  snprintf(dest, nb, "%s", source);
  dest = strpbrk(dest, " \t\n");
  if(dest)
    *dest = 0;
}

/**
   Copies a trimmed versino into newly allocated string
*/
static char * cf_trim_dup(const char * source) 
{
  size_t l = strlen(source);
  char * buf = malloc(l + 1);
  if(! buf)
    return NULL;
  cf_trim_anew(source, buf, l+1);
  return buf;
}

static uid_t cf_get_uid(const char * uid)
{
  char buffer[1024];		/* To trim the string */
  cf_trim_anew(uid, buffer, sizeof(buffer));
  struct passwd * pwd = getpwnam(buffer);
  if(pwd)
    return pwd->pw_uid;

  fprintf(stderr, _("Could not find user named '%s'\n"), buffer);
  return -1;
}

static gid_t cf_get_gid(const char * gid)
{
  char buffer[1024];		/* To trim the string */
  cf_trim_anew(gid, buffer, sizeof(buffer));
  struct group * group = getgrnam(buffer);
  if(group)
    return group->gr_gid;

  fprintf(stderr, _("Could not find group named '%s'\n"), buffer);
  return -1;
}

/**
   Reads a comma-separated list of uid and store it into the pointer
   pointed to by target
 */
static int cf_get_uidlist(char * value, uid_t ** target)
{
  /* First, compute the number of commas in the list. */
  size_t nb = cf_count_chars(value, ",");
  char * end;
  uid_t * vals = malloc(sizeof(uid_t) * (nb+2));
  *target = vals;

  while(1) {
    end = strchr(value, ',');
    if(end)
      *end = 0;
    *vals = cf_get_uid(value);
    if(*vals == -1)
      return -1;		/* Something went wrong */

    vals++;
    if(! end)
      break;
    value = end+1;
  }
  *vals = -1;
  return 0;
}


/**
   Reads a comma-separated list of gids and store it into the pointer
   pointed to by target
 */
static int cf_get_gidlist(char * value, gid_t ** target)
{
  /* First, compute the number of commas in the list. */
  size_t nb = cf_count_chars(value, ",");
  char * end;
  gid_t * vals = malloc(sizeof(gid_t) * (nb+2));
  *target = vals;

  while(1) {
    end = strchr(value, ',');
    if(end)
      *end = 0;
    *vals = cf_get_gid(value);
    if(*vals == -1)
      return -1;		/* Something went wrong */

    vals++;
    if(! end)
      break;
    value = end+1;
  }
  *vals = -1;
  return 0;
}


/**
   Reads a list of strings into the target
*/
static int cf_read_stringlist(char * value, ci_string_list * target)
{
  size_t nb = cf_count_chars(value, ",");
  char * end;
  char ** strings;

  target->strings = malloc(sizeof(char *) * (nb+2));
  if(! target->strings)
    return -1;
  strings = target->strings;
  while(1) {
    end = strchr(value, ',');
    if(end)
      *end = 0;
    *strings = cf_trim_dup(value);
    if(! *strings)
      return -1;
    strings++;
    if(! end)
      break;
    value = end+1;
  }
  *strings = 0;
  return 0;
}

/**
   Assigns the value to the given key, ensuring that the value match.
*/
static int cf_key_assign_value(cf_key * key, char * value)
{
  switch(key->target->type) {
    int val;
  case boolean_item: {
    ci_bool * t = (ci_bool *)key->target->target;
    switch((long)key->info) {
    case 0:			/* Normal */
      if(! cf_get_boolean(value, &val)) {
	ci_bool_set_default(t, val); /* Or directly use the internals ? */
	return 0;
      }
    case 1:			/* Allow_user */
      if(cf_get_uidlist(value, &(t->allowed_users)))
	return -1;
      return 0;
    case 2:			/* Allow_group */
      if(cf_get_gidlist(value, &(t->allowed_groups)))
	return -1;
      return 0;
    case 3:			/* Allow_user */
      if(cf_get_uidlist(value, &(t->denied_users)))
	return -1;
      return 0;
    default:
      return -1;
    }
    return -1;
  }
  case string_list:  {
    ci_string_list * t = (ci_string_list *) key->target->target;
    return cf_read_stringlist(value, t);
  }
  default:
    break;
  }
}


int cf_read_file(FILE * file, cf_spec * specs)
{
  char line_buffer[1000];
  char * name;
  char * value;
  int retval = 0;
  cf_key * keys;

  /* Compile regular expressions when necessary */
  if(cf_prepare_regexps()) 
    return -1;
  keys = cf_spec_build_keys(specs);


  while(! feof(file) && !retval) {
    int line_type;
    cf_key * key;
    if(cf_read_line(file, line_buffer, sizeof(line_buffer))) {
      retval = -1;
      break;
    }
    
    line_type = cf_classify_line(line_buffer, &name, &value);
    switch(line_type) {
    case BLANK_LINE:
      break;
    case DECLARATION_LINE:
      key = cf_key_find(name, keys);
      if(key) {
	if(cf_key_assign_value(key, value)) {
	  retval = -2;
	  break;
	}
      }
      else {
	fprintf(stderr, "Error: key '%s' is unknown\n", name);
	retval = -2;
      }
      break;
    default:
      fprintf(stderr, "Error parsing configuration file line: %s\n", 
	      line_buffer);
      retval = -1;
    }
  }

  cf_key_free_keys(keys);
  return 0;
}
