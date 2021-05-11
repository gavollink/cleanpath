# CleanPath

## What Is It?

For UNIX bash/sh (.profile, .bashrc)

I use multiple UNIX systems, but how does one set `PATH`, `LD_LIBRARY_PATH`,
`CLASSPATH`, and `PERL5LIB` in .profile or .bashrc, keep it clean and
free of duplicaes?

I want a common .profile across systems.  However, not all systems have
the same things installed.  I inherit some stuff from the system itself, so
I want to keep whatever that is.


### Most Common Way

Here is the most common way to deal with PATH...

```sh
    PATH="$HOME/bin:$HOME/sbin:$PATH:/usr/local/bin"
```

If this system doesn't have $HOME/sbin, there's a path-hit fail every time a command is typed.

### More Careful Way

```sh
    if [ -d $HOME/bin ]; then
        PATH=$HOME/bin:$PATH
    fi
    if [ -d $HOME/sbin ]; then
        PATH=$HOME/sbin:$PATH
    fi
    if [ -d /usr/local/bin ]; then
        PATH=$PATH:/usr/local/bin
    fi
    # In reality, this would go on for many, MANY more lines
    echo $PATH
    /home/you/bin:/usr/bin:/usr/sbin:/usr/local/bin
```

If for some reason my profile ran twice, then my PATH would mostly duplicate.

```sh
    echo $PATH
    /home/you/bin:/home/you/bin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/bin
```

### This project but pure shell

Then, I started doing what this project does with shell commands.
That logic loop is a [sight](https://gitlab.home.vollink.com/-/snippets/2)
to see.  Notably, that can't currently deal with a directory name with a
space in it, where cleanpath can.

### Using CleanPath instead

Now my .profile looks like this...

```sh
    export PATH     # cleanpath cannot read what is not exported
    PATH=`cleanpath -Pb -- "${HOME:-x}/bin" "${HOME:-x}/sbin"`
    PATH=`cleanpath -P -- "/usr/local/bin"`
```
In reality, each line is longer, but there are no more individual lines to deal with PATH.
PATH is likely already exported, but this is illustrative of any ENVNAME requested.

```sh
    echo $PATH
    /home/you/bin:/usr/bin:/usr/sbin:/usr/local/bin
```

- cleanpath -P will check each token in the PATH environment variable to
verify it actually exists on the system.
- cleanpath -b puts the command line parameters before the PATH environment
variable in the output.

## Build...

Should work on any UNIX, though MacOS and Linux have been tested
(check [bottom](#compile-on-unix) of this README for more details).

cc -o cleanpath cleanpath.c

or

gcc -Wall -o cleanpath cleanpath.c

## Non-obvious features

- cleanpath always removes dupliates from the combined output.
- cleanpath always removes dupliate delimiters from the output.
- cleanpath removes delimiters from the beginning and end of the output.

## Using With Other Environment Variables

```sh
    LD_LIBRARY_PATH=`cleanpath -Pb LD_LIBRARY_PATH -- "$HOME/lib"`
    export LD_LIBRARY_PATH
    LD_LIBRARY_PATH=`cleanpath -P LD_LIBRARY_PATH -- "/usr/local/lib"`
    export LD_LIBRARY_PATH
```

## Non environment lists

This illustrates using a different separator and not pulling an
environment variable.

```sh
    $ cleanpath -XF, -- abcd efgh ijkl efgh mnop abcd qrst
    abcd,efgh,ijkl,mnop,qrst
```

## Other examples

This is exactly equivalent to above, mixing delimited ADDENV and non-delimited,
separate ADDENV as above.

```sh
    $ cleanpath -XF, -- abcd,efgh ijkl,efgh,mnop abcd qrst
    abcd,efgh,ijkl,mnop,qrst
```

Removing extra delimiters, as above, default delimiter.

```sh
    $ cleanpath -X ::::abcd: :efgh: :ijkl::efgh mnop:abcd: :qrst:::
    abcd:efgh:ijkl:mnop:qrst
```

Space as delimiter

```sh
    $ cleanpath -XF' ' "   abcd efgh ijkl efgh  " mnop abcd qrst
    abcd efgh ijkl mnop qrst
```

## Options

As shown in the examples above, short options can be bundled `-PbF-`

    --before
    -b
        Put ENVADD before the contents of ENVNAME in final output.  Useless
        if --noenv is also used (will warn)
    --exists
    -e
        Verify that each --delimiter separated token exists.
    --checkpaths
    -P
        Verify that each --delimiter separated token is a valid directory.
    --checkfiles
    -f
        Verify that each --delimiter separated token is a valid directory.
    --delimiter :
    -F:
        Single character delimiter for tokens both for output and inputs
        Defaults to colon (:)
    --env
        A very explicit way to set the ENVNAME
    --noenv
    -X
        Do not pull tokens of an environment variable.
        Will warn if --before is specified.

    ENVNAME
        Name of environment variable to pull in
    
    --
        Stops looking for options, past this everything is seen as ENVADD
        Very necessary if an ENVADD starts with a '-'.
    
    ENVADD
        Additional data to evaluate and add to output.
        If delimited in a single argument, the delimiter must match --delimiter
    
    --nosizelimit
    -S
        Normally, if the ENVNAME, an equal sign, and the final output would 
        be longer than the system ARG_MAX, the program will print an error
        and truncate the output to match the ARG_MAX.  This option turns that off.
    --debug
        Prints what it is doing as it happens, good for viewing if the output
        is not what was expected.
    --help
        Not exactly the same as this, but has the same info.
    --license
        Print the LICENSE.
    --vdebug
        This isn't in --help, it's super verbose and only useful for debugging
        changes to this program itself.

## Cautions

- Use export before calling cleanpath, or cleanpath cannot read the 
environment variable.
- Cleanpath will preserve anything it can see that is not the delimiter:
    - `cleanpath -X -- 'abcd' ' abcd'` will output `abcd: abcd`
    - `cleanpath -X -- abcd: abcd` will output `abcd`
    (without quoting, it won't see the space)

## TODO

At some point, I might upgrade this to use [DJB](https://cr.yp.to/) 
strings.

I haven't used csh since the 90s, but it is possible that for this to be 
useful under csh, some big adjustements might need to be made.
If someone asks, I can probably make that happen.

Far off, I might consider auto-adjusting options based on the environment
variable requested.  For example `cleanpath CLASSPATH -- ...` could
invoke different options to the defaults (-e) or (-Pf).

Reach out with requests.

## Deliberately NOT Done

There is no Makefile because this project is self contained.  It doesn't 
even have a header file.

Some options do not have short alternatives.
I set short options for everything that might be required in scripted use.
Not for `--env` because ENVNAME default to the first bareword.

There are MANY features I could add, but this does what I need it to do 
in just less that 1000 lines of C.

## Compile on UNIX

As far as I know, there are only two things this needs from
non-standardized hearder files:

- `ARG_MAX` : the OS character limit for a command line
- `PATH_MAX` : the OS character limit for a full path

Right now I am doing compiler define checks for `__MACH__` (MacOS),
`__linux__`, and `__hpux` (even though I don't have access to this, 
I used to work on it) to try to get this right, falling back to
a generic ask for `limits.h` for anything else.

Fixing this for YOUR system should be easy.  The hard part is finding where
those are defined, and making it happen.  I am curious to know if anybody 
compiles this on another UNIX or a CPU other than x86 or armhf (Pi).
Also, this is a low traffic site, I can probably help if you have issues.

    firstname @ lastname . com
