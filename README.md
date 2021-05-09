# CleanPath

## Build...

MacOS or Linux (so far)

cc -o cleanpath cleanpath.c

or

gcc -Wall -o cleanpath cleanpath.c

## What Is It?

For UNIX bash/sh (.profile, .bashrc)

I want a common .profile across systems.  However, not all systems have
the same things installed.  I inherit some stuff from the system itself, so
I want to keep whatever that is.

Old Way

    if [ -d $HOME/bin ]; then
        PATH=$HOME/bin:$PATH
    fi
    if [ -d $HOME/sbin ]; then
        PATH=$HOME/sbin:$PATH
    fi
    if [ -d /usr/local/bin ]; then
        PATH=$PATH:/usr/local/bin
    fi
    echo $PATH
    /home/you/bin:/usr/bin:/usr/sbin:/usr/local/bin

If for some reason my profile ran twice, then my PATH would mostly duplicate.

    PATH=`cleanpath -Pb -- "${HOME}/bin" "${HOME}/sbin"`
    export PATH
    PATH=`cleanpath -P -- "/usr/local/bin"`
    export PATH
    echo $PATH
    /home/you/bin:/usr/bin:/usr/sbin:/usr/local/bin

- cleanpath -P will check each component in the PATH environment variable to
verify it actually exists on the system.
- cleanpath -b puts the command line parameters before the PATH environment
variable in the output.
- cleanpath always removes dupliates from the combined string.
- cleanpath always removes dupliate ':' separators from the string.

## Problems on another UNIX

There are exactly two things this needs from non-standardized hearder files:

- `ARG_MAX` : the OS character limit for a command line
- `PATH_MAX` : the OS character limit for a full path

Right now I check compiler #ifdef for `__MACH__` (MacOS) and
load <sys/syslimits.h> otherwise, I load <linux/limits.h>

Fixing this for YOUR system should be easy.  The hard part is finding where
those are defined.
