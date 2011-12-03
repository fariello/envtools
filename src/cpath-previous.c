#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/* #include <glib.h> */

#ifdef DEBUG_ON
#    define debug(x) debug_out x
#else
#    define debug(x)
#endif

#define verbose(verbosity,args)                 \
    if(opt_verbosity >= verbosity)              \
        verbose_out args

#define eq(str1,str2) (0 == strcmp(str1,str2))
#define is_null(x)                                      \
    (                                                   \
     NULL == (x)                                        \
                                                    )
#define not_null(x)                                     \
    (                                                   \
     NULL != (x)                                        \
                                                    )
#define mem_alloc(type,symbol,num)                      \
    ((symbol) =                                         \
     ((type)malloc((num) * sizeof(type))))              \

#define fatal_mem_alloc(type,symbol,num)                                \
    if(is_null(mem_alloc(type,symbol,num)))                             \
        fatal("Unable to allocate %u bytes of RAM.\n",((num) * sizeof(type)))

#define str_clone(new,old)                          \
    char * new;                                     \
    {                                               \
        fatal_mem_alloc(char *,new,strlen(old)+1);  \
        strcpy(new,old);                            \
    }

#define toggle(x) x = x ? 0 : 1

#define MAXDIRS 100000

typedef struct dir_array_t {
    unsigned int length;
    char ** dirs;
} dir_array_t;

typedef struct args_array_t {
    unsigned int current;
    unsigned int length;
    unsigned int size;
    char ** args;
} args_array_t;

static char  *progname;
extern char **environ;
static int    opt_verbosity = 0;
static int    opt_check_dir_exists = 0;
static int    opt_only_executable_dirs = 1;
static int    opt_debug_on = 1;
static int    opt_dir_exists = 1;
static int    opt_remove_dupes = 1;
static char   opt_delim = ':';

static uid_t uid;
static gid_t gid;


void debug_out(const char *, ...);
void die(const char *, ...);

void verbose_out(const char *format, ...) {
    va_list attribs;
    va_start(attribs, format);
    vfprintf(stderr,format, attribs);
    va_end(attribs);
}

/* Just to deal with debugging output */
void debug_out(const char *format, ...) {
    if (opt_debug_on) {
        va_list attribs;
        fprintf(stderr,"[DEBUG] ");
        va_start(attribs, format);
        vfprintf(stderr,format, attribs);
        va_end(attribs);
    }
}

/* Print something and exit failure. */
void fatal(const char *format, ...) {
    va_list attribs;
    fprintf(stderr,"[%s] FATAL ERROR: ",progname);
    va_start(attribs, format);
    vfprintf(stderr,format, attribs);
    va_end(attribs);
    fprintf(stderr,"\n[%s] PROGRAM MUST TERMINATE.\n",progname);
    exit(EXIT_FAILURE);
}

static dir_array_t *cpath_new_dir_array(size_t size) {
    dir_array_t * new;
    char ** dirs = NULL;
    fatal_mem_alloc(dir_array_t *,new,1);
    if(size) {
        fatal_mem_alloc(char **,dirs,size+1);
        dirs[size] = NULL;
    }
    new->length = 0;
    new->dirs = dirs;
    return new;
}

static void cpath_add_other_arg(char *arg,args_array_t *args_array) {
    str_clone(new_arg,arg);
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
static args_array_t *cpath_parseargs(int arg_count, char *args[]) {
    int i = 0;
    args_array_t *args_array;
    fatal_mem_alloc(args_array_t *,args_array,1);
    args_array->current = 0;
    args_array->length = 0;
    args_array->size = 0;
    args_array->length = 0;
    for(
        i = 1; /* start at 1, not 0, since args[0] is the string with which
                  this programs was called */
        i < arg_count; /* make sure we're not past the last argument */
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
                fatal("Long args are not implemented yet.\n");
            }
            while(*this_arg) {
                switch(*this_arg) {
                case 'v':
                    opt_verbosity ++;
                    break;
                case 'q':
                    opt_verbosity --;
                    break;
                case 'e':
                    toggle(opt_check_dir_exists);
                    break;
                case 'u':
                    toggle(opt_only_executable_dirs);
                    break;
                case 'D':
                    toggle(opt_debug_on);
                    break;
                case 'r':
                    toggle(opt_remove_dupes);
                    break;
                case 'd':
                    opt_delim = *(this_arg + 1);
                    if(!opt_delim)
                        fatal("-d arg not currently supported use -darg in stead.");
                    break;
                default:
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

void cpath_print_dir_array(dir_array_t *dir_array) {
    unsigned int i = 0,len=dir_array->length;
    for(i=0;i<len;i++) {
        fprintf(stderr," - directory[%03i] = \"%s\"\n", i, dir_array->dirs[i]);
    }
}

void cpath_print_path_array(char *env_name, dir_array_t *dir_array) {
    fprintf(stderr,"PATH Array for ENV{'%s'}:\n",env_name);
    cpath_print_dir_array(dir_array);
}

dir_array_t * get_path_array(char *env_name) {
    dir_array_t * path_dir_array;
    unsigned int dir_count = 0;
    char *path_copy, *end, *ptr, *dir, **path_array;
    debug(("get_path_array(\"%s\"):\n",env_name));
    char *path_string = getenv(env_name);
    
    if(!path_string) {
        return cpath_new_dir_array(0);
    }
    unsigned int path_len = strlen(path_string)+1;
    fatal_mem_alloc(char *,path_copy,path_len);
    strcpy(path_copy, path_string);
    debug(("get_path_array('%c',\"%s\"): PATH = \"%s\"\n",
           opt_delim,env_name,path_copy));
    dir = ptr = path_copy;
    if('\0' != *ptr) {
        /* if there is anything in the PATH value, count the first */
        dir_count++;
        /* loop through all the characters in the PATH value */
        while('\0' != *ptr) {
            /* if it's a delimitor */
            if(opt_delim == *ptr) {
                /* count it */
                dir_count ++;
                /* terminate it */
                *ptr = '\0';
                dir = ptr+1;
            }
            /* next char */
            ptr++;
        }
    }
    fatal_mem_alloc(char **,path_array,dir_count+1);
    path_array[dir_count] = '\0';
    /* If we have anything */
    if(dir_count) {
        /* path_array index */
        unsigned int i = 0;
        /* Start at begining */
        dir = ptr = path_copy;
        /* Set end */
        end = ptr + path_len;
        /* loop through all the characters in the PATH value */
        while(ptr < end) {
            if('\0' == *ptr) {
                path_array[i] = dir;
                i++;
                dir=ptr+1;
            }
            /* next char */
            ptr++;
        }
    }
    path_dir_array = cpath_new_dir_array(dir_count);
    path_dir_array->length = dir_count;
    path_dir_array->dirs = path_array;
    cpath_print_path_array(env_name,path_dir_array);
    /* if we "free" path_array, we need to free *path_array
       which points to path_copy first? */
    return path_dir_array;
}

int cpath_seen_before(char **seen_dirs, char *dir) {
    /* have we seen this directory before? */
    unsigned int seen_before = 0;
    debug(("cpath_seen_before(**seen_dirs@%u,\"%s\"):\n",
           (unsigned int)seen_dirs,dir));
    /* look through all currently seen dirs */
    while(*seen_dirs) {
        /* if we've seen it ... */
        if(eq(*seen_dirs,dir)) {
            /* set seen_before */
            seen_before = 1;
            /* don't bother with the rest */
            break;
        }
        /* next dir */
        seen_dirs++;
    }
    /* if we've not seen this before, we should add it */
    if(!seen_before) {
        /* move to end of array */
        while(*seen_dirs) {
            seen_dirs++;
        }
        /* add this dir */
        *seen_dirs = dir;
        /* null terminate array */
        *(seen_dirs++) = NULL;
    }        
    return seen_before;
}

int cpath_can_exec_dir(struct stat * file_stat) {
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

void cpath_build_path(dir_array_t *dir_array, char *new_path) {
    unsigned int i,len = dir_array->length,new_dir_count;
    struct stat file_stat;
    char **seen_dirs;
    debug(("cpath_build_path(*dir_array@%u,\"%s\"):\n",
           (unsigned int)dir_array,new_path));
    if(opt_only_executable_dirs) opt_dir_exists = 1;
    fatal_mem_alloc(char**,seen_dirs,len+1);
    for(i=0;i<len;i++)
        fatal_mem_alloc(char*,seen_dirs[i],1);
    /* loop through all the directories */
    for(i=0;i<len;i++) {
        char *current_dir = dir_array->dirs[i];
        if( opt_remove_dupes && cpath_seen_before(seen_dirs,current_dir) ) {
            /* don't do anything with this dir */
            debug(("Ignoring previously seen directory \"%s\"\n",
                   current_dir));
        } else if (opt_dir_exists && 0 != stat(current_dir, &file_stat)) {
            /* if we're only supposed to check if directories exist, and it
               doen't we let someone know if needed, and skip it */
            verbose(1,("Ignoring non-existent directory \"%s\"\n",
                       current_dir));
        } else if (opt_only_executable_dirs && /* if we only want executable
                                                  directories */
                   ! cpath_can_exec_dir(&file_stat)
                   ) {
            /* don't do anything with this dir */
            debug(("Ignoring non-usable directory \"%s\"\n",current_dir));
        } else {
            /* we got here, we keep this directory. This is more effiencent than
               strcat, as we don't search the string for null twice, once to add
               the current_dir and once to add ":" */
            while('\0' != *current_dir) {
                *new_path = *current_dir;
                current_dir++;
                new_path++;
            }
            /* count it */
            new_dir_count ++;
            /* add delim to new_path */
            *new_path = opt_delim;
            /* increment new_path only, current_dir gets reset */
            new_path++;
        }
    }
    /* if we ended with a delim, back-up */
    if(opt_delim == *(new_path - 1))
        new_path --;
    /* we have our new path, "close" it out by null-terminating the string */
    *new_path = '\0';
}

void cpath_clean_path(char *env_name) {
    dir_array_t *dir_array;
    char *new_path = '\0', *old_path;
    debug(("cpath_clean_path(\"%s\"):\n",env_name));
    /* get the array of directories */
    dir_array = get_path_array(env_name);
    /* print it out */
    cpath_print_path_array(env_name,dir_array);
    /* keep track of old dir count */
    unsigned int old_dir_count = 0;
    /* keep track of seen directories */
    char ** seen_dirs;
    fatal_mem_alloc(char **,seen_dirs,old_dir_count+1);
    seen_dirs[0]=NULL;
    /* do this only if the PATH value is not empty */
    if(dir_array->length) {
        fatal_mem_alloc(char *,new_path,strlen(old_path)+1);
        cpath_build_path(dir_array,new_path);
    } else {
        new_path = "";
    }
    debug(("NEW PATH %s=\"%s\"\n",env_name,new_path));
}

void _init_prog(int arg_count, char *arguments[]) {
    char * char_ptr = strrchr(arguments[0],'/');
    if(NULL != char_ptr)
        char_ptr++;
    else
        char_ptr = arguments[0];
    fatal_mem_alloc(char *,progname,strlen(char_ptr)+1);
    strcpy(progname,char_ptr);
    debug(("Program name=\"%s\"\n",progname));
}

int main(int arg_count, char *arguments[]) {
    _init_prog(arg_count,arguments);
    uid = getuid();
    gid = getuid();
    args_array_t *env_array = cpath_parseargs(arg_count,arguments);
    unsigned int i,len = env_array->length;
    if(len) {
        for(i=0;i<len;i++)
            cpath_clean_path(env_array->args[i]);
    } else {
        cpath_clean_path("PATH");
    }
    return 0;
}
