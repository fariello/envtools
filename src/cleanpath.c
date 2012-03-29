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
 * Using one static global struct seemed neater than having a lot of static
 * globals or passing a struct around everywhere.
 */
typedef struct path_info_t {
  char * env_name;
  unsigned int path_string_length;
  char * old_path_string;
  char * new_path_string;
  char * new_path_string_ptr;
  unsigned int *directory_hashes;
  unsigned int old_directory_count;
  unsigned int new_directory_count;
  char ** directories;
  char delim;
} path_info_t;
static path_info_t path_info;

/**
 * Command line option settings
 */
static int    opt_verbosity = 0;
static int    opt_debug_on = 0;
static int    opt_all_paths = 0;
static int    opt_check_exists = 1;
static int    opt_only_executable_dirs = 1;
static int    opt_dirs_only = 0;
static int    opt_remove_dupes = 1;
static int    opt_discard_empty = 1;
static int    opt_include_verbose = 0;
static char   opt_delim = ':';
static int    opt_target_shell = CPATH_SHELL_BASH;
static int    opt_output_unchanged = 0;
static int    opt_common_paths = 0;
static args_array_t *opt_exclude_match;

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
    fprintf(debug_fh,"# [DEBUG] ");
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
 * "Known" common paths. Not an extensive list, just the ones that popped in
 * my head plus RUBY ones since some folks use it around here
 */
static char *common_paths[] = {
  "PATH",
  "MANPATH",
  "LD_LIBRARY_PATH",
  "PERL5LIB",
  "PYTHONPATH",
  "RUBYLIB",
  "DLN_LIBRARY_PATH",
  "RUBYLIB_PREFIX",
  "CLASSPATH",
  ""
};

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
         "Cleans up PATH environment variables. By default with no arguments, cleans up\n"
         "only the $PATH environment variable by outputting text which is suitable for\n"
         "inclusion in the shell script (e.g., eval `%s` in bash).\n"
         "\n"
         "By default keeps only the first occurence of each directory in the path, and\n"
         "keeps only existing directories which the current user may actually use (i.e.,\n"
         "execute). Since it is possible to be able to use directories to which the\n"
         "current user does not have read access, those are not discarded.\n"
         "\n"
         "--------------------------------------------------------------------------------\n"
         "OPTIONS:\n"
         "  -A    = Work on all environment variables whose name ends in \"PATH\".\n"
         "  -D    = Toggle on/off debugging output if avaiable (default: off)\n"
         "  -V    = Toggle on/off to print verbose output to stdout for inclusion into \n"
         "          scripts (default: off)\n"
         "  -b    = Print bash/sh/dash set compatible \"export FOO=bar;\" definitions\n"
         "          (default).\n"
         "  -C    = Work on \"common\" environment variables:\n"
         "              PATH, MANPATH, LD_LIBRARY_PATH, PERL5LIB, PYTHONPATH, RUBYLIB,\n"
         "              DLN_LIBRARY_PATH, RUBYLIB_PREFIX, CLASSPATH\n"
         "  -c    = Print tcsh/csh set compatible \"setenv FOO=bar;\" definitions.\n"
         "  -d'X' = Set the path delimiter to 'X' (default ':').\n"
         "  -e    = Toggle on/off to include only existing directories (default: on)\n"
         "  -h    = Print this help message\n"
         "  -?    = Print this help message\n"
         "  -I    = Toggle on/off outputting unchanged variables (default: off)\n"
         "  -n    = Print non-shell set compatible \"FOO=bar\".\n"
         "  -q    = Decrease verbosity by 1. Can be used multiple times.\n"
         "  -r    = Toggle remove duplicate directories (default: on)\n"
         "  -u    = Toggle include only \"usable\" (executable) directories (default: on)\n"
         "  -v    = Increase verbosity by 1. Can be used multiple times.\n"
         "  -x    = Toggle only allow directories (no files). (default: off)\n",
         get_progname(), get_progname());
}

static char *cpath_getval(int *idx, char **current_arg, int argc, char *args[]) {
  char *next_arg = NULL;
  if('\0' != *(*(current_arg) + 1)) {
    next_arg = str_clone(++*(current_arg));
  } else {
    *(idx) += 1;
    if(*(idx) >= argc)
      fatal("ERROR: No argument provided for -%s!\n", *(current_arg));
    if('-' == *(args[*idx]))
      fatal("ERROR: No argument provided for -%s! if '%s' should be the argument, please quote.\n", *(current_arg), args[*idx]);
    if('"' == *(args[*idx]) || '\'' == *(args[*idx])) {
      int len = strlen(args[*idx]);
      if(len > 1) {
        if(*(args[*idx]) == *(args[*idx] + len)) {
          next_arg = str_clone(args[*idx] + 1);
          *(next_arg + len) = '\0';
        }
      }
    }
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
  opt_exclude_match = cpath_new_args_array_t();
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
        case 'A':
          toggle(opt_all_paths);
          break;
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
        case 'e':
          toggle(opt_check_exists);
          break;
        case 'u':
          toggle(opt_only_executable_dirs);
          break;
        case 'x':
          toggle(opt_dirs_only);
          break;
        case 'D':
          toggle(opt_debug_on);
          break;
        case 'C':
          toggle(opt_common_paths);
          break;
        case 'r':
          toggle(opt_remove_dupes);
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
        case 'd':
          opt_delim = *(cpath_getval(&i,&this_arg,argc,args));
          verbose(1,("# Set path delimitor to \"%c\"\n",opt_delim));
          if(!opt_delim) {
            usage();
            fatal("-d arg not currently supported use -darg in stead.");
          }
          break;
        case 'E':
          cpath_add_other_arg(cpath_getval(&i,&this_arg,argc,args),opt_exclude_match);
          verbose(1,("# Exclude path members matching \"%s\"\n",opt_exclude_match));
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

/**
 * Check if we've seen this path before for the current environment variable.
 *
 * @param dir the current directory string to check
 * @param the hash of that directory string. Passing it is more efficient that
 *        recomputing it.
 */
int cpath_seen_before(char *dir, unsigned int hash) {
  debug(3,("cpath_seen_before(\"%s\",%u)\n",dir,hash));
  unsigned int seen_before = 0;
  unsigned int idx = 0;
  /* is there a directory hash that already matches this directory's hash? */
  for(idx=0;idx < path_info.new_directory_count;idx++) {
    debug(3,("cpath_seen_before: compare %u with path_info.directory_hashes[%u]=%u\n",hash,idx,path_info.directory_hashes[idx]));
    if(hash == path_info.directory_hashes[idx]) {
      /* If there was a matching hash, then check if the strings match */
      if(0 == strcmp(dir,path_info.directories[idx])) {
        debug(3,("cpath_seen_before: YES\n"));
        seen_before = 1;
        break;
      } else {
        debug(3,("cpath_seen_before: Hash was the same, directory was different.\n",dir));
      }
    }
  }
  /* if we've not seen this before, we should add it */
  if(!seen_before) {
    debug(3,(" - NOT seen_before \"%s\"; Adding\n",dir));
    /* add this hash */
    path_info.directory_hashes[path_info.new_directory_count] = hash;
    /* add this dir */
    path_info.directories[path_info.new_directory_count] = dir;
    /* increment dir count */
    path_info.new_directory_count ++;
  }        
  return seen_before;
}

/**
 * Check if this is a "usable" directory.
 *
 * @param file_stat the struct stat that resulted from "stat" ing the current
 *        directory. We don't need anything else.
 */
int cpath_can_exec_dir(struct stat * file_stat) {
  debug(3,("cpath_can_exec_dir(struct stat * file_stat)\n"));
  return
    S_ISDIR(file_stat->st_mode) && /* file is a dir  AND */
    /* dir is executable by this process */
    (
     /* most frequent hit first - can everyone execute into this dir? */
     (S_IXOTH & file_stat->st_mode) ||
     /* next most frequent - can my group execute into this dir? */
     (file_stat->st_gid == gid && (file_stat->st_mode & S_IXGRP)) ||
     /* least frequent hit - can I myself execute into this dir? */
     (file_stat->st_uid == uid && (S_IXUSR & file_stat->st_mode))
     );
}

/**
 * Check if we should add (i.e., keep) this directory to the PATH in question
 *
 * @param dir the current directory string to check
 * @param the hash of that directory string. Passing it is more efficient that
 *        recomputing it.
 */
unsigned char cpath_should_add(char *current_file_or_dir, unsigned int hash) {
  struct stat file_stat;
  debug(3,("cpath_should_add(\"%s\",%u)\n",current_file_or_dir,hash));
  /* if we don't keep empty dirs, don't bother with the rest */
  if(opt_discard_empty && '\0' == *current_file_or_dir) {
    verbose(2,("# Ignoring empty string directory name \"%s\"\n",
               current_file_or_dir));
    return 0;
  }
  /* If we're removing stuff, see if this whould be removed. */
  if(opt_exclude_match->length > 0) {
    unsigned int i;
    for(i = 0; i < opt_exclude_match->length; i++) {
      if(NULL != strstr(current_file_or_dir,opt_exclude_match->args[i])) {
        verbose(2,("# Removing \"%s\" (matched '%s')\n",
                   current_file_or_dir, opt_exclude_match->args[i]));
        return 0;
      }
    }
  }
  if( opt_remove_dupes && cpath_seen_before(current_file_or_dir,hash) ) {
    /* don't do anything with this dir */
    verbose(2,("# Ignoring duplicate file or directory \"%s\"\n",
               current_file_or_dir));
    return 0;
  }
  if (opt_only_executable_dirs || opt_check_exists) {
    if(0 != stat(current_file_or_dir, &file_stat)) {
      /* if we're only supposed to check if directories exist, and it
         doen't we let someone know if needed, and skip it */
      verbose(2,("# Ignoring non-existent file or directory \"%s\"\n",
                 current_file_or_dir));
      return 0;
    } else {
      int is_dir = S_ISDIR(file_stat.st_mode);
      if (opt_dirs_only && ! is_dir) {
        verbose(2,("# Ignoring non-directory \"%s\"\n",current_file_or_dir));
        return 0;
      }
      if (opt_only_executable_dirs && /* if we only want executable
                                              directories */
          is_dir &&
          ! cpath_can_exec_dir(&file_stat)
          ) {
        /* don't do anything with this dir */
        verbose(2,("# Ignoring non-usable directory \"%s\"\n",current_file_or_dir));
        return 0;
      }
    }
    return 1;
  } else {
    /* If we got here, we have a keeper */
    verbose(2,("# Keeping \"%s\"\n", current_file_or_dir));
    return 1;
  }
}

/**
 * Add this directory to the new path if and only if we should.
 *
 * @param dir the current directory string to check
 * @param the hash of that directory string. Passing it is more efficient that
 *        recomputing it.
 */
void cpath_add_if(char *current_file_or_dir, unsigned int hash) {
  debug(5,("cpath_add_if: before new_path = \"%s\"\n", path_info.new_path_string));
  { /* Start isolated block */
    char *char_ptr = current_file_or_dir;
    debug(4,("cpath_add_if: - Before trimming: \"%s\"\n", current_file_or_dir));
    /* move to end of string */
    while(*char_ptr) char_ptr ++;
    /* back up one (b/c were at the null char */
    char_ptr --;
    /* back up over all trailing slashes "/" */
    while('/' == *char_ptr ) char_ptr --;
    /* move forward one */
    char_ptr ++;
    /* terminate string here. */
    *char_ptr = '\0';
    debug(4,("cpath_add_if: - After  trimming: \"%s\"\n", current_file_or_dir));
  } /* End isolated block */
  if( cpath_should_add(current_file_or_dir,hash) ){
    debug(3,("Adding \"%s\"\n", current_file_or_dir));
    /* If we're not at the start of the new path string, then we need
       to add a "delim" separator for this directory */
    if(path_info.new_path_string_ptr != path_info.new_path_string) {
      debug(5,(" - adding delim '%c' to new_path=\"%s\"\n", path_info.delim, path_info.new_path_string));
      *(path_info.new_path_string_ptr) = path_info.delim;
      path_info.new_path_string_ptr ++;
    }
    while('\0' != *current_file_or_dir) {
      *(path_info.new_path_string_ptr) = *current_file_or_dir;
      path_info.new_path_string_ptr ++;
      current_file_or_dir ++;
    }
    *(path_info.new_path_string_ptr) = '\0';
    debug(4,(" - After adding_path=\"%s\"\n", path_info.new_path_string));
  } else {
    debug(3,("NOT Adding \"%s\"\n", current_file_or_dir));
  }
  debug(5,("cpath_add_if: after new_path = \"%s\"\n", path_info.new_path_string));
}

/**
 * Clean a given PATH environment variable.
 *
 * @param delim the path delimiter
 * @param env_name the name of the environment variable
 * @param old_path_string the path string as it was before we did anything to it.
 */
void cpath_clean_path(char delim, const char *env_name,const char *old_path_string) {
  path_info.env_name                = NULL;
  path_info.path_string_length      = 0;
  path_info.old_path_string         = NULL;
  path_info.new_path_string         = NULL;
  path_info.new_path_string_ptr     = NULL;
  path_info.directory_hashes        = NULL;
  path_info.old_directory_count     = 0;
  path_info.new_directory_count     = 0;
  path_info.directories             = NULL;
  path_info.delim                   = delim;
  if(! old_path_string) {
    verbose(3,("# OLD %s=\"\" # was unset\n",env_name));
    return;
  }

  verbose(3,("# OLD %s=\"%s\"\n",env_name,old_path_string));
  {  /* Start isolated block */
    /* In one pass, get the original length of the PATH and calculate
       how many directories it contained (needed for malloc'ing stuff
       later */
    const char *old_path_start = old_path_string;
    const char *current_char_ptr = old_path_start;
    while('\0' != *current_char_ptr) {
      if(path_info.delim==*current_char_ptr) {
        path_info.old_directory_count++;
      }
      current_char_ptr++;
    }
    /* keep the path length (including the terminating '\0') around */
    path_info.path_string_length = current_char_ptr - old_path_start + 1;
  } /* End isolated block */

  /* Store an unadulterated copy of the old_path. Was really only using this for
     debugging purposes, and can probably get rid of it, but I'd like to leave
     well enough alone. */
  path_info.old_path_string = (char *)calloc(path_info.path_string_length + 1,1);
  if(! path_info.old_path_string) fatal("Unable to allocate RAM for path copy.");
  strcpy(path_info.old_path_string,old_path_string);

  /* we need anoth copy, since we'll "destroy" this one, and if we use old_path_string
     we'd bee destroying the environment which this code sees. Probably only really
     an issue because I allow people to specify the same environment variable more
     than once.
  */
  char *old_path_string_copy = (char *)calloc(path_info.path_string_length + 1,1);
  if(! old_path_string_copy) fatal("Unable to allocate RAM for path copy 2.");
  strcpy(old_path_string_copy,old_path_string);

  /* Here we'll store the "new" PATH as we build it */
  path_info.new_path_string = (char *)calloc(path_info.path_string_length + 1,1);
  if(! path_info.new_path_string) fatal("Unable to allocate RAM for path copy.");
  path_info.new_path_string_ptr = path_info.new_path_string;

  /* Need to keep track of all of the directories we've seen so far. An arguably
     better (less RAM) way of doing this is just storing them all in the
     path_info.new_path_string, since we know it has enough room and then at then
     end replacing all the null terminators with path_info.delim except for the
     last one. Next iteration of this code.
   */
  path_info.directories = (char **)malloc(sizeof(char*) * (path_info.old_directory_count + 1));
  if(! path_info.directories) fatal("Unable to allocate RAM for seen directories.");
  *path_info.directories = NULL;

  /* Need to keep track of the directory hashes. Comparing one hash int is MUCH
     faster than comparing the whole string. */
  path_info.directory_hashes = (unsigned int *)calloc(sizeof(unsigned int) * (path_info.old_directory_count + 1),1);
  if(! path_info.directory_hashes) fatal("Unable to allocate RAM for seen directories lengths.");

  debug(3,("Building new...\n"));
  { /* Start isolated block */
    /* Start at the begining */
    char *current_file_or_dir = old_path_string_copy;
    /* This is a magic hash number */
    unsigned int hash = 5381;
    /* Loop through all the chars in the PATH string */
    while('\0' != *old_path_string_copy) {
      /* At each delimitor, process the previous directory */
      if(path_info.delim == *old_path_string_copy) {
        *old_path_string_copy = '\0';
        cpath_add_if(current_file_or_dir, hash);
        current_file_or_dir = old_path_string_copy+1;
        /* Reset the hash value to the magic value */
        hash = 5381;
      } else {
        /* Compute the hash */
        hash = ((hash << 5) + hash) + (*old_path_string_copy);
      }
      /* Move to next char */
      old_path_string_copy++;
    }
    /* Still have to process the last directory if any */
    cpath_add_if(current_file_or_dir, hash);
  } /* End isolated block */
  verbose(3,("# NEW %s=\"%s\"\n",env_name,path_info.new_path_string));
  /* Now output a string to STDOUT as asked */
  if(
     opt_output_unchanged ||
     (0 != strcmp(path_info.new_path_string,path_info.old_path_string))
     ) {
    switch(opt_target_shell) {
    case CPATH_SHELL_NONE:
      printf("%s=%s\n",env_name,path_info.new_path_string);
      break;
    case CPATH_SHELL_BASH:
      printf("export %s=%s\n",env_name,path_info.new_path_string);
      break;
    case CPATH_SHELL_CSH:
      printf("setenv %s \"%s\";\n",env_name,path_info.new_path_string);
      break;
    default:
      usage();
      fatal("Unknown target shell '%d'\n",opt_target_shell);
      exit(EXIT_FAILURE);
      break;
    }
  }
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
  unsigned int i,len = env_array->length;
  /* If we're supposed to look at all variables that end in "PATH", then
     do it now. */
  if(opt_all_paths) {
    debug(2,("Looking for all PATH environment variables\n"));
    while(*envp) {
      char *definition = (char*)malloc(sizeof(char) * (strlen(*envp) + 1));
      if(!definition) fatal("Out of memory error. Could not allocate RAM for env definition.\n");
      strcpy(definition,*envp);
      char *env_name = definition;
      char *cptr = definition;
      char *env_value = definition;
      /* Find first '='. NOTE: Should not have to check for '\0' as the envp
         strings ALWAYS have at least on '=' */
      while('=' != *cptr) cptr++;
      /* Terminate env_name */
      *cptr = '\0';
      /* env_value should be everything after that */
      env_value = cptr + 1;
      debug(3,(" - Checking env_name=\"%s\"\n",env_name));
      /* Backup to where PATH would be, if there is enough room
         to back up. Also helps prevent going off in uncharted RAM */
      if(cptr - env_name >= 4) {
        cptr -= 4;
        debug(4,(" - - test string=\"%s\"\n",cptr));
        if(0 == strcmp(cptr,"PATH")) {
          debug(3,(" - - Ends in PATH, will use.\n",env_name));
          cpath_clean_path(opt_delim,env_name,env_value);
        }
      }
      envp++;
    }
  }
  if(opt_common_paths) {
    /* Try to clean the common paths */
    unsigned int i;
    for(i=0; *(common_paths[i]); i++) {
      cpath_clean_path(opt_delim,common_paths[i], getenv(common_paths[i]));
    }
  }
  /* If we were told to do some environment variables on the command-line, do them */
  if(len) {
    for(i=0;i<len;i++)
      cpath_clean_path(opt_delim,env_array->args[i], getenv(env_array->args[i]));
  } else if(! opt_common_paths && ! opt_all_paths) {
      /* If there was nothing else on the command-line, clean "PATH" */
      cpath_clean_path(opt_delim,"PATH", getenv("PATH"));
  }
  return 0;
}
