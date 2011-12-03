/* Make sure we only load this file once by using a define semaphore  */
#ifndef _UTILS_LOADED_SEMAPHORE
#define _UTILS_LOADED_SEMAPHORE

/* Define some "constants" that we might like to use */
#define UTILS_YES 1
#define UTILS_NO  0
#define UTILS_EXIT_GENERAL_FAILURE 1
#define UTILS_EXIT_MALLOC_FAILURE  9
#define UTILS_NO_INIT "ERROR: utils_program_init() never called."

/* Only include debugging output when NDEBUG is not defined */
#ifdef NDEBUG
#    define debug(x,y)
#else
#    define debug(x,y) if(opt_debug_on && opt_verbosity >= x) debug_out y
#endif

/* Saving on function call overhead */
#define verbose(lvl,args) if(opt_verbosity >= lvl) verbose_out args
/* Easier to read shorthand */
#define eq(str1,str2) (0 == strcmp(str1,str2))
/* Easier to read shorthand */
#define is_null(x) (NULL == (x))
/* Easier to read shorthand, more efficient than (0==strlen(x)) */
#define is_empty_str(x) (0 == (*x))
/* Easier to read shorthand */
#define not_null(x) (NULL != (x))

/* Easier to read shorthand for below */
#define _malloc_ptr_size(data_type,data_size) ((data_type *)malloc((data_size) * (data_type)))
/* Easier to read shorthand for below */
#define _init_and_malloc_ptr(data_type,symbol_name,data_size)            \
  ((data_type *)(symbol_name) = malloc_ptr_size( (data_type) , (data_size)))
/* Saving on function call overhead for each malloc */
#define init_and_malloc_ptr(data_type,symbol_name,data_size)      \
  (                                                                     \
   init_and_malloc_ptr((data_type) , (symbol_name) , (data_size));      \
   if(is_null( (symbol_name) ) )                                        \
     fatal_malloc_error( malloc_ptr_size( (data_type) , (data_size)) )  \
                                                                        )
/* Saving on function call overhead for copy of a string */
#define str_clone(new,old)                              \
  (                                                     \
   char * new;                                          \
   fatal_init_and_malloc_ptr(char *,new,strlen(old)+1); \
   strcpy(new,old);                                     \
                                                        )
/* Easier to read shorthand */
#define toggle(x) ( ( x ) = ( x ) ? 0 : 1 )

/* Generic array struct without the C++ overhead */
typedef struct char_array_s {
  unsigned int max_size;
  unsigned int length;
  char ** chars;
} char_array_t;

/* =============================================================================
 * We store the program basename and other info here
 * ========================================================================== */
static char    *utils_prog_basename         = UTILS_NO_INIT;
static char    *utils_prog_start_wd         = UTILS_NO_INIT;
static char    *utils_prog_dirname          = UTILS_NO_INIT;
static char    *utils_prog_abspath          = UTILS_NO_INIT;
static char    *utils_prog_usage            = UTILS_NO_INIT;
/* =============================================================================
 * Controls verbose and debugging output
 * ========================================================================== */
static char    *utils_start_comment         = "";
static char    *utils_end_comment           = "";
static char    *utils_verbose_prepend       = NULL;
static char    *utils_verbose_append        = "";
static char    *utils_debug_prepend         = "";
static char    *utils_debug_append          = "";
static FILE    *utils_verbose_fh            = stderr;
static FILE    *utils_debug_fh              = stderr;
static boolean  utils_multiline_comments    = UTILS_NO;
static boolean  utils_comment_verbose       = UTILS_NO;
static boolean  utils_comment_debug         = UTILS_NO;

void utils_set_sh_comments(const * comment_start, const * comment_end
                           , char ** prepend_str_ptr,  char ** append_str_ptr) {
  utils_multiline_comments = UTILS_NO;
  if(not_null(*prepend_str_ptr))
    free(*prepend_str_ptr);
  if(! eq(comment_end, append_str_ptr
  if(not_null(*append_str_ptr))
    free(*prepend_str_ptr);

  utils_verbose_prepend = "# [%s]INFO[%2d]:"
}
void utils_sh_verbose() {
  utils_comment_verbose = UTILS_YES;
  utils_set_sh_comments();
}
void utils_sh_debug() {
  utils_comment_debug = UTILS_YES;
  utils_set_sh_comments();
}
/* static char  *prog_dir      = NULL; */
/* static char  *prog_real     = NULL; */
int    opt_verbosity = 0;
int    opt_debug = 0;

static void start_comment_if(FILE *fh) {
  if(start_comment)
    fprintf(fh,"%s",start_comment);
}

static void end_comment_if(FILE *fh) {
  if(end_comment)
    fprintf(fh,"%s",end_comment);
}

void verbose_out(const char *format, ...) {
  va_list attribs;
  start_comment_if(verbose_fh);
  va_start(attribs, format);
  vfprintf(verbose_fh,format, attribs);
  va_end(attribs);
  end_comment_if(verbose_fh);
}

/* Just to deal with debugging output */
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

/* Print something and exit failure. */
void fatal(const char *format, ...) {
  va_list attribs;
  start_comment_if(verbose_fh);
  fprintf(verbose_fh,"[%s] FATAL ERROR: ",prog_basename);
  va_start(attribs, format);
  vfprintf(verbose_fh,format, attribs);
  va_end(attribs);
  end_comment_if(verbose_fh);
  start_comment_if(verbose_fh);
  fprintf(verbose_fh,"[%s] PROGRAM MUST TERMINATE. Exiting %d.\n",prog_basename,UTILS_EXIT_GENERAL_FAILURE);
  end_comment_if(verbose_fh);
  exit(UTILS_EXIT_GENERAL_FAILURE);
}

static void cpath_add_other_arg(char *arg,char_array_t *args_array) {
  str_clone(new_arg,arg);
  if(0 == args_array->max_size) {
    args_array->max_size = 64;
    fatal_mem_alloc(char **,args_array->chars,64);
  } else if(args_array->max_size == args_array->length) {
    args_array->max_size = args_array->max_size * 2;
    args_array->chars = realloc( args_array->chars,
                                args_array->length* 2 * sizeof(char **) );
  }
  args_array->chars[args_array->length] = new_arg;
  args_array->length++;
}

void sh_comments() {
  start_comment = "# ";
  end_comment = NULL;
}

void c_comments() {
  start_comment = "/* ";
  end_comment = " */";
}

void no_comments() {
  start_comment = NULL;
  end_comment = NULL;
}

void set_verbose_out(FILE *fh) {
  verbose_fh = fh;
}

void set_debug_out(FILE *fh) {
  debug_fh = fh;
}

static void _init_() {
  verbose_fh = stderr;
  debug_fh = stderr;  
}

char * get_progname() {
  return prog_basename;
}

void init_prog_light(char **argv) {
  char *basename_ptr = NULL,*cptr = *argv;
  unsigned int len = 0;
  _init_();
  if(! cptr)
    fatal("Could not determine program name.");
  if('\0' == *cptr)
    fatal("This program has no name. Should not be possible.");
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

#endif