# Helpful GNU-Linux environment tools.

## Installing

```bash
git clone https://github.com/fariello/genvtools.git
cd genvtools
make
```

The executables will in in the `bin` directory.

# Tools

## cleanpath

Self documenting in theory. See **Help** below.


### Motivation

Sometimes PATH and other environmental variables get out of hand. I
created a super light-weight tools to find and remove duplicates and
"bad" directories or libraries that can be used in bash-like or
c-shell like environments.

**Why?**

Many scripts add to environment variables and do so blindly. If you
run scripts that call scripts that source files on multiple systems
created by many people, you can end up with very long variables often
with multiple copies directories or directories that no-longer exist
or broken links in your library or executable search paths. This can
cause several types of issues such as the variables getting truncated
(too long), the systems (especially shared HPC with automount
capabilities) to try and mount non-existent mounts, and general
slowness when searching through paths for libraries and executables.

There are other ways of doing this, but I wanted a c-program that took
no startup time, did not initialize or read files, etc. that was
multi-shell-version compatible and would not slow things down. When
you're counting milliseconds, the startup of Python, Perl, awk,
etc. processes to accomplish this in poorly designed nested-script
environments is just too much.

In very complex pipelines created by many people, it was not uncommon
for shell-script inception to occur ( script1 calls script2 calls
... script33 ) in loops. Ensuring that the bottom script had

```
# Clean up all paths
`cleanpath -A`
```

At the top made everything run a little faster and smoother. I now
include this in by `.bashrc` and/or `.profile` files.

### Help

Help example:

```bash
> bin/cleanpath -h
```
```text
================================================================================
Usage: cleanpath OPTIONS [ENV_NAME1 [ENV_NAME2 [...]]]

Cleans up PATH environment variables. By default with no arguments, cleans up
only the $PATH environment variable by outputting text which is suitable for
inclusion in the shell script (e.g., eval `cleanpath` in bash).

By default keeps only the first occurence of each directory in the path, and
keeps only existing directories which the current user may actually use (i.e.,
execute). Since it is possible to be able to use directories to which the
current user does not have read access, those are not discarded.

Note that all environment variables specified on the command line are checked,
regardless of whether or not their names match the criteria. Only variables
in the current environment are checked for criteria. For efficiency reasons, we
don't check to see if we previously processed an environment variable, so a
variable may be processed muliple times. This results in improved efficiency
under most cases, but reduced efficiency if a variable is specified on the
command line and matches criteria. Not a significant difference, however.

--------------------------------------------------------------------------------
OPTIONS:

Environment Variable NAME Criteria:
  -A    = Work on all environment variables whose name ends in "PATH".
  -C    = Work on "common" environment variables:
              PATH, MANPATH, LD_LIBRARY_PATH, PERL5LIB, PYTHONPATH, RUBYLIB,
              DLN_LIBRARY_PATH, RUBYLIB_PREFIX, CLASSPATH

Environment Variable VALUE Criteria:
  -E st = Remove path elements that match the string 'st'. Can specify multiple.
          Sorry, no regular expressions supported (yet).
  -d'X' = Set the path delimiter to 'X' (default ':').
Output Formatting:
  -b    = Print bash/sh/dash set compatible "export FOO=bar;" definitions
          (default).
  -c    = Print tcsh/csh set compatible "setenv FOO=bar;" definitions.
  -D    = Toggle on/off debugging output if avaiable (default: off)
  -e    = Toggle on/off to include only existing directories (default: on)
  -I    = Toggle on/off outputting unchanged variables (default: off)
  -k    = Toggle on/off keeping empty PATH components
  -n    = Print non-shell set compatible "FOO=bar".
  -q    = Decrease verbosity by 1. Can be used multiple times.
  -r    = Toggle remove duplicate directories (default: on)
  -u    = Toggle include only "usable" (executable) directories (default: on)
  -v    = Increase verbosity by 1. Can be used multiple times.
  -V    = Toggle on/off to print verbose output to stdout for inclusion into 
          scripts (default: off)
  -x    = Toggle only allow directories (no files). (default: off)

Help:
  -h or -? = Print this help message
--------------------------------------------------------------------------------
Example usage in bash:

# "Clean" the PATH variable ENV_NAME1 (even if it's not a PATH variable it
# will be treated as one, plus any "common" path variables, and while cleaning
# them, remove any path elements that match /some/path
# Note the backticks ("`"). These are there since cleanpath cannot modify
# the parents environment and by wrapping the output of cleanpath in backticks
# it tells the shell (e.g, bash in this case) to execute the commands outputted
# by cleanpath
`cleanpath ENV_NAME1 -E /some/path`

# Don't remove, just show me the output from above:
cleanpath ENV_NAME1 -E /some/path

================================================================================
```

## unsetenvs

Much like cleanpath this is a super light-weight way to find and unset environment variables in multiple shells with the same motivation. See built-in help.

### Help

Help example:

```bash
> bin/unsetenvs -h
```
```text
================================================================================
Usage: unsetenvs OPTIONS [ENV_NAME1 [ENV_NAME2 [...]]]

Unset environment variables. All environment variables specified on the command
line are unset, regardless of whether or not they match criteria. Only variables
in the current environment are checked for criteria. Note that for efficiency, we
don't check to see if an environment variable was previously unset by us, so a
variable may be unset muliple times.
--------------------------------------------------------------------------------
OPTIONS:

Environment Variable NAME Criteria:
  -m st = Unset any env variable whose name matches the string 'st'.
  -s st = Unset any env variable whose name starts with the string 'st'.
  -e st = Unset any env variable whose name ends with the string 'st'.

Environment Variable VALUE Criteria:
  -M st = Unset any env variable whose value matches the string 'st'.
  -S st = Unset any env variable whose value starts with the string 'st'.
  -E st = Unset any env variable whose value ends with the string 'st'.

Output Formatting:
  -I    = Toggle on/off outputting unchanged variables (default: off)
  -V    = Toggle on/off to print verbose output to stdout for inclusion into 
          scripts (default: off)
  -b    = Print bash/sh/dash set compatible "export FOO=bar;" definitions
          (default).
  -c    = Print tcsh/csh set compatible "setenv FOO=bar;" definitions.
  -n    = Print non-shell set compatible "FOO=bar".
  -x    = Toggle exporting in bash/sh/dah. Export: "export FOO=",
          Not: "unset FOO". (default: export)
  -q    = Decrease verbosity by 1. Can be used multiple times.
  -v    = Increase verbosity by 1. Can be used multiple times.

Help:
  -h or -? = Print this help message
--------------------------------------------------------------------------------
Example usage in bash:

# Remove ENV_NAME1 and any env variable whose value matches /some/path:
# Note the backticks ("`"). These are there since unsetenvs cannot modify
# the parents environment and by wrapping the output of unsetenvs in backticks
# it tells the shell (e.g, bash in this case) to execute the commands outputted
# by unsetenvs
`unsetenvs ENV_NAME1 -M /some/path`

# Don't remove, just show me the output from above:
unsetenvs ENV_NAME1 -M /some/path

================================================================================
```