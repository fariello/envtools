#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "utils.c"

typedef struct path_info_t {
  char * env_name;
  unsigned int path_string_length;
  char * old_path_string;
  char * new_path_string;
  unsigned int directory_hashes_length;
  unsigned int *directory_hashes;
  unsigned int directories_length;
  char ** directories;
} path_info_t;


/* static path_info_t path_info; */

static int    opt_all_paths = 0;
static int    opt_check_dir_exists = 0;
static int    opt_only_executable_dirs = 1;
static int    opt_dir_exists = 1;
static int    opt_remove_dupes = 1;
static char   opt_delim = ':';

static uid_t uid;
static gid_t gid;


static char **seen_dirs = NULL;
static int *seen_dir_hashes = NULL;

static args_array_t *cpath_parseargs(int argc, char *args[]) {
  int i = 0;
  args_array_t *args_array;
  fatal_mem_alloc(args_array_t *,args_array,1);
  args_array->length = 0;
  args_array->size = 0;
  args_array->length = 0;
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
        fatal("Long args are not implemented yet.\n");
      }
      while(*this_arg) {
        switch(*this_arg) {
        case 'a':
          toggle(opt_all_paths);
          break;
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

int cpath_seen_before(char *dir, int hash) {
  debug(3,("cpath_seen_before(\"%s\",%d)\n",dir,hash));
  /* have we seen this directory before? */
  unsigned int seen_before = 0;
  int idx = 0;
  char **seen_dirs_ptr = seen_dirs;
  for(idx=0;seen_dir_hashes[idx];idx++) {
    debug(3,("cpath_seen_before: compare %d with seen_dir_hashes[%d]=%d\n",hash,idx,seen_dir_hashes[idx]));
    if(hash == seen_dir_hashes[idx]) {
      debug(3,("cpath_seen_before: Maybe\n",dir));
      seen_before = 1;
      break;
    }
  }
  if(seen_before) {
    seen_before = 0;
    debug(3,("cpath_seen_before(\"%s\"):\n",dir));
    /* look through all currently seen dirs */
    while(*seen_dirs_ptr) {
      debug(3,(" - comparing \"%s\" to \"%s\"\n",dir,*seen_dirs_ptr));
      /* if we've seen it ... */
      if(eq(*seen_dirs_ptr,dir)) {
        /* set seen_before */
        seen_before = 1;
        /* don't bother with the rest */
        break;
      }
      /* next dir */
      seen_dirs_ptr++;
    }
  }
  /* if we've not seen this before, we should add it */
  if(!seen_before) {
    debug(3,(" - NOT seen_before \"%s\"; Adding\n",dir));
    /* add this dir */
    *seen_dirs_ptr = dir;
    /* move up one */
    seen_dirs_ptr++;
    /* null terminate array */
    *seen_dirs_ptr = NULL;
    /* keep hash around */
    seen_dir_hashes[idx] = hash;
  }        
  debug(3,("cpath_seen_before? %s\n",(seen_before?"YES":"NO")));
  return seen_before;
}

int cpath_can_exec_dir(struct stat * file_stat) {
  debug(3,("cpath_can_exec_dir(struct stat * file_stat@%ul)\n",(unsigned long long)file_stat));
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

unsigned char cpath_should_add(char *current_dir, int hash) {
  struct stat file_stat;
  debug(3,("cpath_should_add(\"%s\",%d)\n",current_dir,hash));
  if( opt_remove_dupes && cpath_seen_before(current_dir,hash) ) {
    /* don't do anything with this dir */
    verbose(2,("Ignoring previously seen directory \"%s\"\n",
               current_dir));
    return 0;
  } else if (opt_dir_exists && 0 != stat(current_dir, &file_stat)) {
    /* if we're only supposed to check if directories exist, and it
       doen't we let someone know if needed, and skip it */
    verbose(2,("Ignoring non-existent directory \"%s\"\n",
               current_dir));
    return 0;
  } else if (opt_only_executable_dirs && /* if we only want executable
                                            directories */
             ! cpath_can_exec_dir(&file_stat)
             ) {
    /* don't do anything with this dir */
    verbose(2,("Ignoring non-usable directory \"%s\"\n",current_dir));
    return 0;
  } else {
    /* If we got here, we have a keeper */
    verbose(2,("Keeping \"%s\"\n", current_dir));
    return 1;
  }
}

void cpath_add_if(char * old_path_string, char **new_path_char_ptr, char delim,
                  char *current_dir, int hash) {
  { /* Start isolated block */
    char *char_ptr = current_dir;
    debug(4,("cpath_add_if: Before trimming: \"%s\"\n", current_dir));
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
    debug(4,("cpath_add_if: After  trimming: \"%s\"\n", current_dir));
  } /* End isolated block */
  if( cpath_should_add(current_dir,hash) ){
    debug(3,("Adding \"%s\"\n", current_dir));
    if(current_dir != old_path_string) {
      *(*new_path_char_ptr) = delim;
      (*new_path_char_ptr) ++;
    }
    while('\0' != *current_dir) {
      *(*new_path_char_ptr) = *current_dir;
      (*new_path_char_ptr) ++;
      current_dir ++;
    }
  } else {
    debug(3,("NOT Adding \"%s\"\n", current_dir));
  }
}

void cpath_clean_path(char delim,char *env_name,char *old_path_string) {
  unsigned int len = 0,dir_count = 0;
  char *new_path_string = NULL;
  if(! old_path_string) {
    verbose(1,("OLD %s=\"\" # was unset\n",env_name));
    return;
  }
  verbose(1,("OLD %s=\"%s\"\n",env_name,old_path_string));
  {  /* Start isolated block */
    /* In one pass, get the original length of the PATH and calculate
       how many directories it contained (needed for malloc'ing stuff
       later */
    char *old_path_start = old_path_string;
    char *current_char_ptr = old_path_start;
    while('\0' != *current_char_ptr) {
      if(delim==*current_char_ptr) {
        dir_count++;
      }
      current_char_ptr++;
    }
    /* keep the path length (including the terminating '\0') around */
    len = current_char_ptr - old_path_start + 1;
  } /* End isolated block */
  new_path_string = (char *)malloc(len + 1);
  if(! new_path_string) fatal("Unable to allocate RAM for path copy.");
  *new_path_string='\0';

  seen_dirs = (char **)malloc(sizeof(char*) * (dir_count + 1));
  if(! seen_dirs) fatal("Unable to allocate RAM for seen directories.");
  *seen_dirs = NULL;

  seen_dir_hashes = (int *)calloc(sizeof(int) * (dir_count + 1),1);
  if(! seen_dir_hashes) fatal("Unable to allocate RAM for seen directories lengths.");

  debug(3,("Building new...\n"));
  {  /* Start isolated block */
    /* In one more pass, calculate directory hashes, build up the new path as needed */
    char *old_path_char_ptr = old_path_string;
    char *new_path_char_ptr = new_path_string;
    char *current_dir = old_path_char_ptr;
    int hash = 5381;
    dir_count = 0;
    while('\0' != *old_path_char_ptr) {
      if(delim == *old_path_char_ptr) {
        dir_count ++;
        *old_path_char_ptr = '\0';
        debug(5,(" - before new_path = \"%s\"\n", new_path_string));
        cpath_add_if(old_path_string,&new_path_char_ptr,delim,
                     current_dir, hash);
        current_dir = old_path_char_ptr+1;
        debug(5,(" - after  new_path = \"%s\"\n", new_path_string));
        seen_dir_hashes[dir_count] = hash;
        dir_count++;
        hash = 5381;
      } else {
        hash = ((hash << 5) + hash) + (*old_path_char_ptr);
      }
      old_path_char_ptr++;
    }
    *new_path_char_ptr = '\0';
    debug(5,(" - before new_path = \"%s\"\n", new_path_string));
    cpath_add_if(old_path_string,&new_path_char_ptr,delim,
                 current_dir, hash);
    current_dir = old_path_char_ptr+1;
    debug(5,(" - after  new_path = \"%s\"\n", new_path_string));
  }  /* End isolated block */
  verbose(1,("NEW %s=\"%s\"\n",env_name,new_path_string));
}

int main(int argc, char *argv[], char *envp[] ) {
  init_prog_light(argv);
  uid = getuid();
  gid = getuid();
  args_array_t *env_array = cpath_parseargs(argc,argv);
  unsigned int i,len = env_array->length;
  if(opt_all_paths) {
    debug(2,("Looking for all PATH environment variables\n"));
    while(*envp) {
      char *env_name = *envp;
      char *cptr = *envp;
      char *env_value = *envp;
      /* Find first '='. NOTE: Should not have to check for '\0' as the envp
         strings ALWAYS have at least on '=' */
      while('=' != *cptr) cptr++;
      /* Terminate env_name */
      *cptr = '\0';
      /* env_value should be everything after that */
      env_value = cptr + 1;
      debug(3,(" - Checking env_name=\"%s\"\n",env_name));
      cptr -= 4;
      debug(4,(" - - test string=\"%s\"\n",cptr));
      if(0 == strcmp(cptr,"PATH")) {
        debug(3,(" - - Ends in PATH, will use.\n",env_name));
        cpath_clean_path(opt_delim,env_name,env_value);
      }
      envp++;
    }
  } else {
    if(len) {
      for(i=0;i<len;i++)
        cpath_clean_path(opt_delim, env_array->args[i], getenv(env_array->args[i]));
    } else {
      cpath_clean_path(opt_delim,"PATH", getenv("PATH"));
    }
  }
  return 0;
}
