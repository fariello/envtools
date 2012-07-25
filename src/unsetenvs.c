#define __WORDSIZE 64
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


#ifndef EXIT_FAILURE
#    define EXIT_FAILURE 1
#endif

#ifdef DEBUG_ON
#    define debug(x,y) if(opt_debug_on && opt_verbosity >= x) debug_out y
#else
#    define debug(x,y)
#endif

#define verbose(verbosity,args)                 \
  if(opt_verbosity >= verbosity)                \
    verbose_out args

#define eq(str1,str2) (0 == strcmp(str1,str2))
#define is_null(x)                                      \
  (                                                     \
   NULL == (x)                                          \
                                                      )
#define not_null(x)                                     \
  (                                                     \
   NULL != (x)                                          \
                                                      )
#define mem_alloc(type,symbol,num)              \
  ((symbol) =                                   \
   ((type)malloc((num) * sizeof(type))))        \

#define fatal_mem_alloc(type,symbol,num)                                \
  if(is_null(mem_alloc(type,symbol,num)))                               \
    fatal("Unable to allocate %u bytes of RAM.\n",((num) * sizeof(type)))

#define toggle(x) ( ( x ) = ( x ) ? 0 : 1 )


/**
 * Struct for generic arguments array
 */
typedef struct args_array_t {
  unsigned int size;
  unsigned int length;
  char ** args;
} args_array_t;

/**
 * We store the program basename here
 */
static char  *prog_basename = NULL;
static char *start_comment = NULL;
static char *end_comment = NULL;
FILE *verbose_fh = NULL;
FILE *debug_fh = NULL;

/**
 * Some defines to deal with output
 */
#define CPATH_SHELL_NONE 0
#define CPATH_SHELL_BASH 1
#define CPATH_SHELL_CSH  2

/**
 * Command line option settings
 */
static int    opt_verbosity = 0;
static int    opt_debug_on = 0;
static int    opt_include_verbose = 0;
static int    opt_target_shell = CPATH_SHELL_BASH;
static int    opt_output_unchanged = 0;
static args_array_t *opt_name_match;
static args_array_t *opt_name_starts;
static args_array_t *opt_name_ends;
static args_array_t *opt_value_match;
static args_array_t *opt_value_starts;
static args_array_t *opt_value_ends;

/**
 * Print the start of a comment if needed
 *
 * @param fh the "file handle" to which to print
 */
static void start_comment_if(FILE *fh) {
  if(start_comment)
    fprintf(fh,"%s",start_comment);
}

/**
 * Print the end of a comment if needed
 *
 * @param fh the "file handle" to which to print
 */
static void end_comment_if(FILE *fh) {
  if(end_comment)
    fprintf(fh,"%s",end_comment);
}

/**
 * Print "verbose" output. Basically a rewritten fprintf that "knows"
 * where to print and if it should encapsulate output in comments
 */
void verbose_out(const char *format, ...) {
  va_list attribs;
  start_comment_if(verbose_fh);
  va_start(attribs, format);
  vfprintf(verbose_fh,format, attribs);
  va_end(attribs);
  end_comment_if(verbose_fh);
}

/**
 * Print "debugging" output. Basically a rewritten fprintf that "knows"
 * where to print and if it should encapsulate output in comments. Also
 * prepends output with "[DEBUG] " to make it clear what's going on.
 */
void debug_out(const char *format, ...) {
  if (opt_debug_on) {
    va_list attribs;
    start_comment_if(debug_fh);
    fprintf(debug_fh,"[DEBUG] ");
    va_start(attribs, format);
    vfprintf(stderr,format, attribs);
    va_end(attribs);
    end_comment_if(debug_fh);
  }
}

/**
 * Print a hopefully useful message and exit the program non-zero
 */
void fatal(const char *format, ...) {
  va_list attribs;
  start_comment_if(verbose_fh);
  fprintf(verbose_fh,"[%s] FATAL ERROR: ",prog_basename);
  va_start(attribs, format);
  vfprintf(verbose_fh,format, attribs);
  va_end(attribs);
  end_comment_if(verbose_fh);
  start_comment_if(verbose_fh);
  fprintf(verbose_fh,"[%s] PROGRAM MUST TERMINATE. Exiting %d.\n",prog_basename,EXIT_FAILURE);
  end_comment_if(verbose_fh);
  exit(EXIT_FAILURE);
}

/**
 * Same as malloc, but on failure prints a meaningful error and exits
 * program non-zero
 *
 * @author Gabriele Fariello
 *
 * @param size resize to which size?
 *
 * @return a pointer of type void * to the memory allocated.
 */
void * fatal_malloc(size_t size) {
  void * ptr = (void *)malloc(size);
  if(NULL == ptr) {
    fatal("Out of memory. Failed to allocate %u bytes or RAM.\n",size);
  }
  return ptr;
}

/**
 * Create a new copy (clone) of a string.
 *
 * @author Gabriele Fariello
 *
 * @param string to clone
 *
 * @return a pointer to the new string copy
 */
char * str_clone(char *old_str) {
  char *new_str = NULL;
  new_str = (char *)fatal_malloc(strlen(old_str) + 1);
  strcpy(new_str,old_str);
  return new_str;
}

/**
 * Add another argument to the args_array "array"
 *
 * @param arg the string to add.
 * @args_array the args_array_t to which to add it.
 */
static void cpath_add_other_arg(char *arg,args_array_t *args_array) {
  char * new_arg = str_clone(arg);
  if(0 == args_array->size) {
    args_array->size = 64;
    fatal_mem_alloc(char **,args_array->args,64);
  } else if(args_array->size == args_array->length) {
    args_array->size = args_array->size * 2;
    args_array->args = realloc( args_array->args,
                                args_array->length* 2 * sizeof(char **) );
  }
  args_array->args[args_array->length] = new_arg;
  args_array->length++;
}

/**
 * Set the verbose output format to be shell comments (i.e., prepend
 * "# " to them.
 */
void sh_comments() {
  start_comment = "# ";
  end_comment = NULL;
}

/**
 * Set the verbose output format to be c-like comments (i.e., wrap the
 * output in "\/\* " and " \*\/"
 */
void c_comments() {
  start_comment = "/* ";
  end_comment = " */";
}
/**
 * Don't do anything to verbose output, just write it as is.
 */
void no_comments() {
  start_comment = NULL;
  end_comment = NULL;
}

/**
 * Set the filehandle to which verbose output it sent.
 *
 * @param fh the FILE * filehandle
 */
void set_verbose_out(FILE *fh) {
  verbose_fh = fh;
}

/**
 * Set the filehandle to which debugging output it sent.
 *
 * @param fh the FILE * filehandle
 */
void set_debug_out(FILE *fh) {
  debug_fh = fh;
}

/**
 * Initialize any variables needed for the default settings.
 */
static void _init_() {
  verbose_fh = stderr;
  debug_fh = stderr;  
}

/**
 * Get the basename of the currently running program.
 *
 * @return the program's basename
 */
char * get_progname() {
  return prog_basename;
}

/**
 * Initialize things that need it before doing stuff.
 *
 * @param argv the programs argument list
 */
void init_prog_light(char **argv) {
  char *basename_ptr = NULL,*cptr = *argv;
  unsigned int len = 0;
  _init_();
  if(! cptr)
    fatal("Could not determine program name.");
  if('\0' == *cptr)
    fatal("This program has no name. Should not be possible.");
  basename_ptr = cptr;
  while('\0' != *cptr) {
    if('/' == *cptr) {
      len = 0;
      basename_ptr = cptr + 1;
    } else {
      len ++;
    }
    cptr++;
  }
  if(! len)
    fatal("An unknown error occurred when trying to determine the program basename.");
  len = strlen(basename_ptr);
  prog_basename = (char *)malloc(len + 1);
  if(! prog_basename)
    fatal("Out of Memory trying to get RAM for prog_basename.");
  cptr = prog_basename;
  while(*basename_ptr) {
    *cptr = *basename_ptr;
    cptr++;
    basename_ptr++;
  }
  *cptr = '\0';
  debug(2,("Program name=\"%s\"\n",prog_basename));
}


/**
 * Set once for the process, used potentially many times
 */
static uid_t uid;
static gid_t gid;


/**
 * Prints out a (hopefully) useful help menssage
 */
static void usage() {
  printf("Usage: %s OPTIONS [ENV_NAME1 [ENV_NAME2 [...]]]\n"
         "\n"
         "Unset environment variables"
         "--------------------------------------------------------------------------------\n"
         "OPTIONS:\n"
         "  -m st = Unset any environment variable whose name matches the string 'st'.\n"
         "  -s st = Unset any environment variable whose name starts with the string 'st'.\n"
         "  -e st = Unset any environment variable whose name ends with the string 'st'.\n"
         "  -M st = Unset any environment variable whose value matches the string 'st'.\n"
         "  -S st = Unset any environment variable whose value starts with the string 'st'.\n"
         "  -E st = Unset any environment variable whose value ends with the string 'st'.\n"
         "\n"
#ifdef DEBUG_ON
         "  -D    = Toggle on/off debugging output if avaiable (default: off)\n"
#endif
         "  -I    = Toggle on/off outputting unchanged variables (default: off)\n"
         "  -V    = Toggle on/off to print verbose output to stdout for inclusion into \n"
         "          scripts (default: off)\n"
         "  -b    = Print bash/sh/dash set compatible \"export FOO=bar;\" definitions\n"
         "          (default).\n"
         "  -c    = Print tcsh/csh set compatible \"setenv FOO=bar;\" definitions.\n"
         "  -n    = Print non-shell set compatible \"FOO=bar\".\n"
         "  -q    = Decrease verbosity by 1. Can be used multiple times.\n"
         "  -v    = Increase verbosity by 1. Can be used multiple times.\n"
         "\n"
         "  -h or -? = Print this help message\n"
         , get_progname());
}

static char *cpath_getval(int *idx, char **current_arg, int argc, char *args[]) {
  char *next_arg = NULL;
  // If this is in the form -Abcdf, then use bcdf as the value of -A
  if('\0' != *(*(current_arg) + 1)) {
    next_arg = str_clone(++*(current_arg));
  } else {
    // Use next argv as value
    *(idx) += 1;
    // If we've gone past the end of the arguments, we have a problem
    if(*(idx) >= argc)
      fatal("ERROR: No argument provided for -%s!\n", *(current_arg));
    // If the next argument is an argument specifier (starts with
    // '-'), then we have a problem.
    if('-' == *(args[*idx]))
      fatal("ERROR: No argument provided for -%s! if '%s' should be the argument, please quote.\n", *(current_arg), args[*idx]);
    // If it's quoted, then all is good, get the unquoted value.
    if('"' == *(args[*idx]) || '\'' == *(args[*idx])) {
      int len = strlen(args[*idx]);
      if(len > 1) {
        if(*(args[*idx]) == *(args[*idx] + len)) {
          next_arg = str_clone(args[*idx] + 1);
          *(next_arg + len) = '\0';
        }
      }
    }
    // If we have not assinged (a quoted value), just assign the whole thing.
    if(NULL == next_arg)
      next_arg = str_clone(args[*idx]);
  }
  return next_arg;
}

static args_array_t * cpath_new_args_array_t(void) {
  args_array_t *args_array = NULL;
  args_array = (args_array_t *)fatal_malloc(sizeof(args_array));
  args_array->length = 0;
  args_array->size = 0;
  args_array->length = 0;
  return args_array;
}

/**
 * Parse the arguments provided on the command line.
 */
static args_array_t *cpath_parseargs(int argc, char *args[]) {
  int i = 0;
  args_array_t *args_array = cpath_new_args_array_t();
  opt_name_match = cpath_new_args_array_t();
  opt_name_starts = cpath_new_args_array_t();
  opt_name_ends = cpath_new_args_array_t();
  opt_value_match = cpath_new_args_array_t();
  opt_value_starts = cpath_new_args_array_t();
  opt_value_ends = cpath_new_args_array_t();
  for(
      i = 1; /* start at 1, not 0, since args[0] is the string with which
                this programs was called */
      i < argc; /* make sure we're not past the last argument */
      i++ /* increment the argument index */
      ) {
    char *this_arg = args[i];
    if('-' == *this_arg) { /* if it starts with a '-', assume it's an
                              argument for now */
      this_arg ++; /* discard the '-' by incrementing the pointer to the
                      next character in the string */
      if('\0' == *this_arg) { /* if the arg was only one "-", keep it */
        cpath_add_other_arg(this_arg - 1, args_array);
        continue; /* move on. Nothing more to see here. */
      }
      if('-' == *this_arg) { /* if the argument was a "long" agument as *
                                specified by using --argname, handle it *
                                differently */
        /* Not implemented yet */
        usage();
        fatal("Long args are not implemented yet.\n");
      }
      while(*this_arg) {
        switch(*this_arg) {
        case 'v':
          opt_verbosity ++;
          verbose(2,("# Set verbosity to \"%d\"\n",opt_verbosity));
          break;
        case 'h':
        case '?':
          usage();
          exit(0);
          break;
        case 'q':
          opt_verbosity --;
          verbose(2,("# Set verbosity to \"%d\"\n",opt_verbosity));
          break;
        case 'D':
          toggle(opt_debug_on);
          break;
        case 'I':
          toggle(opt_output_unchanged);
          break;
        case 'b':
          opt_target_shell = CPATH_SHELL_BASH;
          break;
        case 'c':
          opt_target_shell = CPATH_SHELL_CSH;
          break;
        case 'n':
          opt_target_shell = CPATH_SHELL_NONE;
          break;
        case 'V':
          toggle(opt_include_verbose);
          if(opt_include_verbose)
            sh_comments();
          else
            no_comments();
          break;
        case 'm':
          cpath_add_other_arg(cpath_getval(&i,&this_arg,argc,args),opt_name_match);
          verbose(1,("# Unset any environment variable whose name matches \"%s\"\n",opt_name_match));
          break;
        case 's':
          cpath_add_other_arg(cpath_getval(&i,&this_arg,argc,args),opt_name_starts);
          verbose(1,("# Unset any environment variable whose name starts with \"%s\"\n",opt_name_starts));
          break;
        case 'e':
          cpath_add_other_arg(cpath_getval(&i,&this_arg,argc,args),opt_name_match);
          verbose(1,("# Unset any environment variable whose name ends with \"%s\"\n",opt_name_ends));
          break;
        case 'M':
          cpath_add_other_arg(cpath_getval(&i,&this_arg,argc,args),opt_value_match);
          verbose(1,("# Unset any environment variable whose value matches \"%s\"\n",opt_value_match));
          break;
        case 'S':
          cpath_add_other_arg(cpath_getval(&i,&this_arg,argc,args),opt_value_starts);
          verbose(1,("# Unset any environment variable whose value starts with \"%s\"\n",opt_value_starts));
          break;
        case 'E':
          cpath_add_other_arg(cpath_getval(&i,&this_arg,argc,args),opt_value_match);
          verbose(1,("# Unset any environment variable whose value ends with \"%s\"\n",opt_value_ends));
          break;

        default:
          usage();
          fatal("Unknown parameter -%c\n",*this_arg);
          break;
        }
        this_arg++;
      }
    } else {
      cpath_add_other_arg(this_arg, args_array);
    }
  }
  return args_array;
}


void set_env(const char *env_name, const char *env_value) {
  switch(opt_target_shell) {
  case CPATH_SHELL_NONE:
    printf("%s=%s\n",env_name,env_value);
    break;
  case CPATH_SHELL_BASH:
    printf("export %s=%s\n",env_name,env_value);
    break;
  case CPATH_SHELL_CSH:
    printf("setenv %s \"%s\";\n",env_name,env_value);
    break;
  default:
    usage();
    fatal("Unknown target shell '%d'\n",opt_target_shell);
    exit(EXIT_FAILURE);
    break;
  }
}
int unset_env(const char *env_name) {
  switch(opt_target_shell) {
  case CPATH_SHELL_NONE:
    printf("%s=\n",env_name);
    break;
  case CPATH_SHELL_BASH:
    printf("unset %s\n",env_name);
    break;
  case CPATH_SHELL_CSH:
    printf("unsetenv %s;\n",env_name);
    break;
  default:
    usage();
    fatal("Unknown target shell '%d'\n",opt_target_shell);
    exit(EXIT_FAILURE);
    break;
  }
  return 1;
}

int unset_value_if(const char *env_name, const char *env_value) {
  unsigned int i;
  debug(3,(" - Checking env_name=\"%s\", env_value=\"%s\"\n",env_name,env_value));
  if(opt_value_match->length > 0) {
    for(i=0;i<opt_value_match->length;++i) {
      if(strstr(env_value,opt_value_match->args[i])) {
        verbose(1,("%s's value matched '%s'\n",env_name,opt_value_match->args[i]));
        return unset_env(env_name);
      }
    }
  }
  if(opt_value_starts->length > 0) {
    for(i=0;i<opt_value_starts->length;++i) {
      if(strstr(env_value,opt_value_starts->args[i]) == env_value) {
        verbose(1,("%s's value began with '%s'\n",env_name,opt_value_match->args[i]));
        return unset_env(env_name);
      }
    }
  }
  if(opt_value_starts->length > 0) {
    char *value_end = env_value + strlen(env_value);
    for(i=0;i<opt_value_starts->length;++i) {
      char *match_str = opt_value_starts->args[i];
      // Could be off end.
      char *value_target = value_end - strlen(match_str);
      // If the match_str fits inside the env_value string and is at the end
      if(value_target > env_value && strstr(env_value,match_str) == value_target) {
        verbose(1,("%s's value ended with '%s'\n",env_name,opt_value_match->args[i]));
        return unset_env(env_name);
      }
    }
  }
  return 0;
}

int unset_name_if(const char *env_name) {
  unsigned int i;
  debug(3,(" - Checking env_name=\"%s\"\n",env_name));
  if(opt_name_match->length > 0) {
    for(i=0;i<opt_name_match->length;++i) {
      if(strstr(env_name,opt_name_match->args[i])) {
        verbose(1,("'%s' matched '%s'\n",env_name,opt_name_match->args[i]));
        return unset_env(env_name);
      }
    }
  }
  if(opt_name_starts->length > 0) {
    for(i=0;i<opt_name_starts->length;++i) {
      if(strstr(env_name,opt_name_starts->args[i]) == env_name) {
        verbose(1,("'%s' begins with '%s'\n",env_name,opt_name_starts->args[i]));
        return unset_env(env_name);
      }
    }
  }
  if(opt_name_starts->length > 0) {
    char *name_end = env_name + strlen(env_name);
    for(i=0;i<opt_name_starts->length;++i) {
      char *match_str = opt_name_starts->args[i];
      // Could be off end.
      char *name_target = name_end - strlen(match_str);
      // If the match_str fits inside the env_name string and is at the end
      if(name_target > env_name && strstr(env_name,match_str) == name_target) {
        verbose(1,("'%s' ends with '%s'\n",env_name,opt_name_ends->args[i]));
        return unset_env(env_name);
      }
    }
  }
  return 0;
}

/**
 * Run this program!
 *
 * @param argc the command-line argument count, including the program name
 * @param argv the command-line argument values, including the program name
 * @param envp the environment variables as "NAME=vALUE" strings.
 */
int main(int argc, char *argv[], char *envp[] ) {
  /* Just initialize some stuff in utils.c */
  init_prog_light(argv);
  /* set the globals. Not really sure if this helps or hurts relative to calling
     getuid()/getgid() for each dir */
  uid = getuid();
  gid = getgid();
  /* Get all the stuff on the command-line that was not an option */
  args_array_t *env_array = cpath_parseargs(argc,argv);
  /* If we're supposed to include the "verbose" output to STDOUT,
     set it as such in utils.c */
  if(opt_include_verbose)
    set_verbose_out(stdout);
  /* Some vars */
  size_t buffer_len = 1024;
  char *buffer = (char*)fatal_malloc(sizeof(char) * buffer_len);
  char *env_def = NULL;
  char *env_name = NULL;
  char *tmp_ptr = NULL;
  unsigned char check_name = opt_name_match->length > 0 || opt_name_starts->length > 0 || opt_name_ends->length > 0;
  unsigned char check_value = opt_value_match->length > 0 || opt_value_starts->length > 0 || opt_value_ends->length > 0;
  switch(opt_target_shell) {
  case CPATH_SHELL_NONE:
    no_comments();
    break;
  case CPATH_SHELL_BASH:
  case CPATH_SHELL_CSH:
    sh_comments();
    break;
  default:
    usage();
    fatal("Unknown target shell '%d'\n",opt_target_shell);
    exit(EXIT_FAILURE);
    break;
  }
  while(*envp) {
    env_def = *envp;
    envp++;
    unsigned int env_len = strlen(env_def) + 1;
    if(buffer_len < env_len) {
      buffer = (char *)realloc(buffer,sizeof(char) *  env_len);
      buffer_len = env_len;
      if(!buffer) fatal("Out of memory error. Could not allocate RAM for env definition.\n");
    }
    debug(3,(" - Checking env=\"%s\"\n",env_def));
    env_name = env_def;
    tmp_ptr = env_def;
    /* Find first '='. NOTE: Should not have to check for '\0' as the envp
       strings ALWAYS have at least on '=' */
    while('=' != *tmp_ptr) tmp_ptr++;
    /* Terminate env_name */
    *tmp_ptr = '\0';
    if(check_name) {
      if(unset_name_if(env_name))
        continue;
    }
    if(check_value) {
      if(unset_value_if(env_name,tmp_ptr+1))
        continue;
    }
    if(opt_output_unchanged) {
      set_env(env_name,tmp_ptr+1);
    }
  }
  return 0;
}
