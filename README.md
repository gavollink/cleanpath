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
    # This goes on for many more lines
    echo $PATH
    /home/you/bin:/usr/bin:/usr/sbin:/usr/local/bin

If for some reason my profile ran twice, then my PATH would mostly duplicate.

Now my .profile looks like this...

    PATH=`cleanpath -Pb -- "${HOME}/bin" "${HOME}/sbin"`
    export PATH
    PATH=`cleanpath -P -- "/usr/local/bin"`
    export PATH

    # Each line is longer, but there are no more lines
    $ echo $PATH
    /home/you/bin:/usr/bin:/usr/sbin:/usr/local/bin

- cleanpath -P will check each component in the PATH environment variable to
verify it actually exists on the system.
- cleanpath -b puts the command line parameters before the PATH environment
variable in the output.
- cleanpath always removes dupliates from the combined string.
- cleanpath always removes dupliate ':' separators from the string.

## Other Environment Variables

    LD_LIBRARY_PATH=`cleanpath -Pb LD_LIBRARY_PATH -- "$HOME/lib"`
    export LD_LIBRARY_PATH
    LD_LIBRARY_PATH=`cleanpath -P LD_LIBRARY_PATH -- "/usr/local/lib"`
    export LD_LIBRARY_PATH

## Non environment lists (different seperator)...

    $ cleanpath -XF, -- abcd efgh ijkl efgh mnop abcd qrst
    abcd,efgh,ijkl,mnop,qrst
    $ cleanpath -XF, -- abcd,efgh ijkl,efgh,mnop abcd qrst
    abcd,efgh,ijkl,mnop,qrst

## Problems Compiling on some other UNIX

There are exactly two things this needs from non-standardized hearder files:

- `ARG_MAX` : the OS character limit for a command line
- `PATH_MAX` : the OS character limit for a full path

Right now I am doing compiler define checks for `__MACH__` (MacOS),
`__linux__`, and `__hpux` to try to get this right, falling back to
a generic ask for `limits.h` for anything else.

Fixing this for YOUR system should be easy.  The hard part is finding where
those are defined, and making it happen.
