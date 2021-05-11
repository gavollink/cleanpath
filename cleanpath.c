/****************************************************************************
 * cleanpath.c
 *
 * Linux/MacOS utility to clean a colon separated ENV variable.
 *
 * LICENSE: Embedded at bottom...
 */
#define CP_VERSION "1.03.02"

#include <stdio.h>          // printf
#include <stdlib.h>         // exit, malloc, free
// These two are ONLY used for strerror(errno) on mem allocation failure.
#include <errno.h>
#include <string.h>
// stat()
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
// ARG_MAX, PATH_MAX
#ifdef __MACH__
    // MacOS file is here
#include <sys/syslimits.h>
#elif defined (__hpux)
    // HP/UX file /may/ be here
#include <sys/param.h>
    // or here
#include <limits.h>
#ifndef PATH_MAX
    // or here...
#define PATH_MAX MAXPATHLEN
#endif
#elif defined (__linux__)
#include <linux/limits.h>
    // Add More, If you have them
#else
    // Something on the internet suggested that this covers Solaris
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX MAXPATHLEN
#endif
#endif

struct options {
    int     exist;
    int     file;
    int     dir;
    int     before;
    int     debug;
    int     sizewarn;
    char    delimiter;
    char    env[ARG_MAX];
    char    extra[ARG_MAX];
};

int     check_opt( struct options *opt, int argc, char *argv[] );
void    help(char *me);
void    usage(char *me);
void    printlicense();
int     strzindex( const char seek, const char *str, int start, int strlen );
int     strzlengthn( const char *str, int strlen );
int     strzcpyn( char *dest, const char *src, int destlen );
int     strzcpynn( char *dest, const char *src, int destlen, int srclen );
int     strzcatn( char *dest, const char *src, int destlen );
int     strzcatnn( char *dest, const char *src, int destlen, int srclen );
int     strneqstrn( const char *seek, int seeklen, const char *str, int strlen );
void    default_opt( struct options *opt );
void    set_exist( struct options *opt, const char *arg, const int val );
void    set_dir( struct options *opt, const char *arg, const int val );
void    set_file( struct options *opt, const char *arg, const int val );
void    set_before( struct options *opt, const char *arg, const int val );
void    set_noenv( struct options *opt, const char *arg, const int val );
void    set_env( struct options *opt, const char *arg, const char *val );
char *  elim_mult( char *str, int strlen, struct options *opt );

#define S_I_ALL (S_IFMT|S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)

int
main( int argc, char *argv[] )
{
    // An env variable can be near ARG_MAX in size.
    // Extras on command line can ALSO be near ARG_MAX in size...
    int memblk = ( ( ARG_MAX * 2 ) + 4 );
    char* holdenv;
    char* origenv;
    struct options opts;

    // init opt structure with defaults
    default_opt( &opts );
    // Set options
    check_opt( &opts, argc, argv );

    holdenv = malloc( memblk );
    if ( holdenv == NULL ) {
        printf( "Unable to allocate memory: %s\n", strerror(errno) );
        exit(5);
    }
    memset(holdenv, 0, memblk);

    origenv = getenv(opts.env);

    if ( opts.before ) {
        // Concat any command-line extras into holdenv
        strzcatnn(
                holdenv,
                opts.extra,
                memblk,
                ARG_MAX
                );
        // Add a delimiter to holdenv
        strzcatnn( holdenv, &opts.delimiter, memblk, 1 );
        // Cat requested ENV VAR onto holdenv
        strzcatn( holdenv, origenv, memblk );
    }
    else {
        // Copy requested ENV VAR into holdenv
        strzcatn( holdenv, origenv, memblk );
        // Add a delimiter to holdenv
        strzcatnn( holdenv, &opts.delimiter, memblk, 1 );
        // Concat any command-line extras into holdenv
        strzcatnn(
                holdenv,
                opts.extra,
                memblk,
                ARG_MAX
                );
    }

//printf("F: %s\n", holdenv);

    // Eliminate multiple :
    elim_mult( holdenv, memblk, &opts );
//printf("E: %s\n", holdenv);

    int    out_s_i = 0;
    int    out_n_i = 0;
    int    inn_s_i = 0;
    int    inn_n_i = 0;

    struct stat statbuf;
    char        path[PATH_MAX];
    int         statret;
    while ( out_n_i < strzlengthn(holdenv, memblk) ) {
        //memset( &statbuf, 0, sizeof(statbuf) );
        out_n_i = strzindex(opts.delimiter, holdenv, out_s_i, memblk);
        if ( out_n_i < out_s_i ) {
            break;
        }
        if ( !strzlengthn((holdenv+out_s_i), (memblk - out_s_i) ) ) {
            break;
        }
        strzcpynn(path, (holdenv+out_s_i), PATH_MAX, (out_n_i - out_s_i) );
        if ( opts.debug ) {
            fprintf( stderr, "EVALUATE (%d) [%s]\n", out_s_i, path );
        }
        if ( opts.exist || opts.file || opts.dir ) {
            statret = stat( path, &statbuf );
            int modefail = 0;
            if ( -1 == statret ) {
                if ( opts.debug ) {
                    fprintf( stderr, "Not exists: \"%s\"\n", path );
                }
                modefail = 7;
            }
            /* file and dir checks below here, everything else, add above */
            else if ( opts.file && opts.dir ) {
                /* If BOTH are set, BOTH of these have to fail */
                if (   ( S_IFDIR != ( statbuf.st_mode & S_IFMT ) )
                    && ( S_IFREG != ( statbuf.st_mode & S_IFMT ) ) )
                {
                    if ( opts.debug ) {
                        fprintf( stderr, "Not a regular file or dir: \"%s\"\n",
                            path );
                    }
                    modefail = 3;
                }
            }
            else if ( ( opts.file )
                && ( S_IFREG != ( statbuf.st_mode & S_IFMT ) ) )
            {
                if ( opts.debug ) {
                    fprintf( stderr, "Not a regular file: \"%s\"\n", path );
                }
                modefail = 2;
            }
            else if ( ( opts.dir )
                && ( S_IFDIR != ( statbuf.st_mode & S_IFMT ) ) )
            {
                if ( opts.debug ) {
                    fprintf( stderr, "Not a directory: \"%s\"\n", path );
                }
                modefail = 1;
            }
            if ( modefail ) {
                strzcpynn( (char *)(holdenv+out_s_i),   // dest
                    (char *)(holdenv+out_n_i+1),        // src
                    memblk - out_s_i,                   // destlen
                    memblk - out_n_i - 1);              // srclen
                if ( opts.debug ) {
                    fprintf( stderr,
                        "Removed token: [%s] leaving [%s] (starting over)\n",
                        path, holdenv );
                }
                out_s_i = out_n_i = 0;
                continue;
            }
        }
        inn_n_i = inn_s_i = (out_n_i + 1);
        while ( inn_n_i < strzlengthn(holdenv, memblk) ) {
            inn_n_i = strzindex(opts.delimiter, holdenv, inn_s_i, memblk);
            if ( 2 <= opts.debug ) {
                fprintf( stderr, "DUP EVAL (%d) [%s]\n",
                    inn_s_i, (char *)(holdenv+inn_s_i));
            }
            if ( inn_n_i < inn_s_i ) {
                break;
            }
            if ( !strzlengthn((holdenv+inn_s_i), (memblk - inn_s_i) ) ) {
                break;
            }
            if ( strneqstrn( (char *)(holdenv+out_s_i), // seekstr
                    (out_n_i - out_s_i),                // seeklen
                    (char *)(holdenv+inn_s_i),          // str
                    (inn_n_i - inn_s_i) ) )             // strlen
            {
                if ( opts.debug ) {
                    fprintf( stderr, "duplicate path: [%s] (removing)\n",
                        path );
                }
                strzcpynn( (char *)(holdenv+inn_s_i),   // dest
                    (char *)(holdenv+inn_n_i+1),        // src
                    memblk - inn_s_i,                   // destlen
                    memblk - inn_n_i );                 // srclen
            }
            inn_s_i = inn_n_i + 1;
        }
        out_s_i = out_n_i + 1;
    }

    if ( opts.delimiter == holdenv[strzlengthn(holdenv, memblk)-1] ) {
        holdenv[strzlengthn(holdenv, memblk)-1] = (char)0;
    }

    int outmax = ( ARG_MAX - 1);
    if ( *opts.env ) {
        outmax = ( outmax - (strzlengthn(opts.env, PATH_MAX) + 1) );
    }

    if (   ( opts.sizewarn )
        && ( outmax < strzlengthn(holdenv, memblk) ) )
    {
        fprintf( stderr,
            "WARN: Output size (%d) larger than %d, truncated\n",
            strzlengthn(holdenv, memblk), outmax );
        holdenv[outmax] = (char)0;
    }
    printf( "%s\n", holdenv );
    free( holdenv );
    exit(0);
}

int
check_opt( struct options *opt, int argc, char *argv[] )
{
    int argcx;
    int askhelp    = 0;
    int asklicense = 0;
    int haveenv    = 0;
    int argFEatsArg = 0;

    // Read command line options...
    for ( argcx = 1; argcx < argc; argcx++ ) {

        // Long Options, anything that starts with --
        if ( strneqstrn( "--", 2, argv[argcx], 2 ) ) {
            if ( 2 <= opt->debug ) {
                fprintf( stderr, "CMDLINE %d [%s]\n", argcx, argv[argcx] );
            }
            if ( strneqstrn( "--before", strlen("--before"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                set_before( opt, argv[argcx], 1 );
            }
            else if ( strneqstrn( "--exists", strlen("--exists"),
                        argv[argcx], strlen(argv[argcx]) ) )
            {
                set_exist( opt, argv[argcx], 1 );
            }
            else if ( strneqstrn( "--checkpaths", strlen("--checkpaths"),
                        argv[argcx], strlen(argv[argcx]) ) )
            {
                set_dir( opt, argv[argcx], 1 );
            }
            else if ( strneqstrn( "--checkfiles", strlen("--checkfiles"),
                        argv[argcx], strlen(argv[argcx]) ) )
            {
                set_file( opt, argv[argcx], 1 );
            }
            else if ( strneqstrn( "--noenv", strlen("--noenv"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                set_noenv( opt, argv[argcx], 1 );
                haveenv = 1;
            }
            else if ( strneqstrn( "--env", strlen("--env"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                if ( argcx + 1 < argc ) {
                    set_env( opt, argv[argcx], argv[argcx+1] );
                    argcx++;
                }
                else {
                    usage(argv[0]);
                    exit(2);
                }
                haveenv = 1;
            }
            else if ( strneqstrn( "--nosizelimit", strlen("--nosizelimit"),
                        argv[argcx], strlen(argv[argcx]) ) )
            {
                opt->sizewarn = 0;
            }
            else if ( strneqstrn( "--license", strlen("--license"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                asklicense = 1;
            }
            else if ( strneqstrn( "--help", strlen("--help"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                askhelp = 1;
            }
            else if ( strneqstrn( "--vdebug", strlen("--vdebug"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                if ( 2 != opt->debug ) {
                    /* Extreme startover */
                    default_opt(opt);
                    opt->debug = 2;
                    fprintf( stderr, "    --vdebug is go\n" );
                    return check_opt( opt, argc, argv );
                }
            }
            else if ( strneqstrn( "--debug", strlen("--debug"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                opt->debug = 1;
            }
            else if ( strneqstrn( "--delimiter", strlen("--delimiter"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                if ( ( argcx + 1 < argc )
                    && ( 1 == strlen(argv[argcx+1]) ) )
                {
                    argcx++;
                    opt->delimiter = argv[argcx][0];
                }
                else {
                    usage(argv[0]);
                    exit(2);
                }
            }
            else if ( strneqstrn( "--", strlen("--"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                break;
            }
            else {
                fprintf( stderr, "Unrecognized option '%s'\n", argv[argcx] );
                usage(argv[0]);
                exit(2);
            }
        }
        else if ( strneqstrn( "-", 1, argv[argcx], 1 ) ) {
            if ( 2 <= opt->debug ) {
                fprintf( stderr, "CMDLINE %d [%s]\n", argcx, argv[argcx] );
            }
            if ( 1 >= strlen( argv[argcx] ) ) {
                fprintf( stderr, "Unrecognized option '%s'\n", argv[argcx] );
                usage(argv[0]);
                exit(2);
            }
            int cx;
            int needf = 0;
            for ( cx = 1; cx < strlen( argv[argcx] ); cx++ ) {
                if ( 'e' == argv[argcx][cx] ) {
                    set_exist( opt, "-e", 1 );
                }
                else if ( 'P' == argv[argcx][cx] ) {
                    set_dir( opt, "-P", 1 );
                }
                else if ( 'f' == argv[argcx][cx] ) {
                    set_file( opt, "-f", 1 );
                }
                else if ( 'b' == argv[argcx][cx] ) {
                    set_before( opt, "-b", 1 );
                }
                else if ( 'S' == argv[argcx][cx] ) {
                    opt->sizewarn = 0;
                }
                else if ( 'F' == argv[argcx][cx] ) {
                    needf = 1;
                }
                else if ( 'X' == argv[argcx][cx] ) {
                    set_noenv( opt, "-X", 1 );
                    haveenv = 1;
                }
                else if ( 'h' == argv[argcx][cx] ) {
                    askhelp = 1;
                }
                else if ( needf ) {
                    opt->delimiter = argv[argcx][cx];
                    needf = 0;
                }
                else {
                    fprintf( stderr,
                        "Unrecognized option '-%c'\n", argv[argcx][cx] );
                    usage(argv[0]);
                    exit(2);
                }
            }
            if ( needf ) {
                if ( argcx + 1 >= argc )
                {
                    fprintf( stderr,
                        "No delimiter given for '-F'\n" );
                    usage(argv[0]);
                    exit(2);
                }
                else if ( 1 != strlen(argv[argcx+1]) ) {
                    fprintf( stderr,
                        "Delimiter given for '-F', needs length 1 (got '%s')\n",
                        argv[argcx+1] );
                    usage(argv[0]);
                    exit(2);
                }
                else {
                    ++argFEatsArg;
                    argcx++;
                    opt->delimiter = *argv[argcx];
                }
            }
        }
    }

    // Keep reading options?
    int goopt   = 1;

    // Loop 2 - Read non-option tokens
    for ( argcx = 1; argcx < argc; argcx++ ) {
        if ( goopt && strneqstrn( "-", 1, argv[argcx], 1 ) ) {
            if ( goopt && strneqstrn( "--", strlen("--"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                goopt = 0;
            }
            else if ( strneqstrn( "--env", strlen("--env"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                argcx++;
            }
            else if ( strneqstrn( "--delimiter", strlen("--delimiter"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                argcx++;
            }
            else if ( strneqstrn( "--delimiter", strlen("--delimiter"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                argcx++;
            }
            else if ( goopt && argFEatsArg
                && ( 0 == strneqstrn( "--", 2, argv[argcx], 2 ) ) )
            {
                int cx;
                for (cx = 1; cx < strlen(argv[argcx]); cx++ ) {
                    if ( 'F' == argv[argcx][cx] ) {
                        argFEatsArg--;
                        argcx++;
                    }
                }
            }
        }
        else {
            // BUG: If delimiter changes during processing,
            // everything will mess up reading extra.
            if ( ( !haveenv ) && ( goopt ) ) {
                if ( 2 <= opt->debug ) {
                    fprintf( stderr, "CMDLINE %d [%s] as ENVNAME\n",
                        argcx, argv[argcx] );
                }
                strzcpyn( opt->env, argv[argcx], ARG_MAX );
                haveenv = 1;
            }
            else {
                if ( 2 <= opt->debug ) {
                    fprintf( stderr, "CMDLINE %d [%s] as ENVADD\n",
                        argcx, argv[argcx] );
                }
                strzcatnn(
                    opt->extra, &opt->delimiter,
                    ARG_MAX,    1
                );
                strzcatn( opt->extra, argv[argcx], ARG_MAX );
            }
        }
    }

    if ( opt->before && ( ! *opt->env ) ) {
        fprintf( stderr, "%s\n", "WARN: --before meaningless with --noenv" );
    }

    if ( opt->debug ) {
        fprintf( stderr, "     --exists: %d\n",
            (opt->exist|opt->file|opt->dir) );
        fprintf( stderr, " --checkpaths: %d\n", opt->dir );
        fprintf( stderr, " --checkfiles: %d\n", opt->file );
        fprintf( stderr, "  --delimiter:'%c'\n", opt->delimiter );
        fprintf( stderr, "     --before: %d\n", opt->before );
        fprintf( stderr, "--nosizelimit: %d\n", !opt->sizewarn );
        fprintf( stderr, "      ENVNAME: %s\n", *opt->env?opt->env:"\t(none)" );
        fprintf( stderr, "       ENVADD: %s\n", opt->extra );
    }
    if ( askhelp | asklicense ) {
        if ( askhelp ) {
            help(argv[0]);
        }
        if ( asklicense ) {
            printlicense();
        }
        exit(0);
    }
    return 1;
}

void
usage(char *me)
{
    fprintf( stderr,
        "   Usage: %s [-ePfXbS] [-F:] [ [ENVNAME] [--] ENVADD ]\n", me);
    fprintf( stderr, "For help: %s -h\n", me);
    return;
}

void
help(char *me)
{
    printf( "Help output for: %s\n", me);
    printf( "\n" );
    printf( "%s\n",
        "Linux/MacOS utility to clean a delimited ENV variable." );
    printf( "%s\n",
        "de-duplicated and processed contents printed to stdout." );
    printf( "\n" );
    printf(
        "   Usage: %s [-ePfXbS] [-F:] [ [ENVNAME] [--] ENVADD ]\n", me);
    printf( "\n" );
    printf( " Example: PATH=`%s -Pb -- \"${HOME:-x}/bin\"`\n", me );
    printf( "    ...add ~/bin to the start of PATH, if it exists.\n" );
    printf( "\n" );
    printf( "\t%s\n",
        "--exists|-e" );
    printf( "\t\t%s\n",
                "Verify that each token exists in the filespace." );
    printf( "\t\t%s\n",
                "Implied by --checkfiles and --checkpaths" );
    printf( "\t%s\n",
        "--checkfiles|-f" );
    printf( "\t\t%s\n",
                "Verify that each token is a regular file." );
    printf( "\t%s\n",
        "--checkpaths|-P" );
    printf( "\t\t%s\n",
                "Verify that each token as a valid directory." );
    printf( "\t%s\n",
        "--delimiter|-F" );
    printf( "\t\t%s\n",
                "Single character delimiter of tokens." );
    printf( "\t\t%s\n",
                "Default is colon (:)" );
    printf( "\t%s\n",
        "--env ENVNAME" );
    printf( "\t\t%s\n",
                "Explicit setting of ENVNAME." );
    printf( "\t%s\n",
        "--noenv|-X" );
    printf( "\t\t%s\n",
                "Do not pull contents of any environment variable" );
    printf( "\t%s\n",
        "--before|-b" );
    printf( "\t\t%s\n",
                "Put ENVADD tokens before ENV in output." );
    printf( "\n" );
    printf( "\t%s\n",
        "ENVNAME" );
    printf( "\t\t%s\n",
                "Name of environment variable to evaluate." );
    printf( "\t\t%s\n",
                "If supplied must come before ENVADD and --." );
    printf( "\t\t%s\n",
                "Disable with --noenv|-X." );
    printf( "\t\t%s\n",
                "Default is PATH" );
    printf( "\t%s\n",
        "--" );
    printf( "\t\t%s\n",
                "Stops looking for options, every further string" );
    printf( "\t\t%s\n",
                "is treated as ENVADD." );
    printf( "\t%s\n",
        "ENVADD" );
    printf( "\t\t%s\n",
                "Extra data to add to ENV for output." );
    printf( "\n" );
    printf( "\t%s\n",
        "--nosizelimit|-S" );
    printf( "\t\t%s\n",
                "Do not print warning and shorten if processed string" );
    printf( "\t\t%s\n",
                "+ ENVNAME + '=' is too large for a command line (ARG_MAX)." );
    printf( "\t\t%s\n",
                "Useful as pipe source." );
    printf( "\t%s\n",
        "--help|-h" );
    printf( "\t\t%s\n",
                "This help text." );
    printf( "\t%s\n",
        "--license" );
    printf( "\t\t%s\n",
                "Show License." );
    printf( "\t%s\n",
        "--debug" );
    printf( "\t\t%s\n",
                "Enable debugging output." );
    /* Deliberately hiding --vdebug */
    printf( "\n" );
    printf( "Version: %s\n", CP_VERSION );
    printf( "\t(System ARG_MAX: %d,  PATH_MAX: %d)\n", ARG_MAX, PATH_MAX );
    printf( "\n" );
    printf( "Copyright (c) 2021 Gary Allen Vollink -- MIT License\n" );
    return;
}

char *
elim_mult( char *str, int strlen, struct options *opt )
{
    int    crow = 0;
    char * d    = str;
    char * s    = str;

    while ( ( ( s - str ) < strlen )
        && ( opt->delimiter == s[0] ) )
    {
        ++s;
    }

    for( ; (s - str) <= (strzlengthn(str, strlen)+1); s++ ) {
        if ( opt->delimiter == s[0] ) {
            crow++;
            if ( (char)0 == s[1] ) {
                break;
            }
        }
        else {
            crow = 0;
        }
        if ( 1 >= crow ) {
            if ( *s != *d ) {
                *d = *s;
            }
            ++d;
        }
    }

    while ( s >= d ) {
        *d = (char)0;
        ++d;
    }
    return str;
}


void
default_opt( struct options *opt )
{
    // init opt structure with defaults
    opt->exist = 0;
    opt->file = 0;
    opt->dir = 0;
    opt->before = 0;
    opt->debug = 0;
    opt->sizewarn = 1;
    opt->delimiter = ':';
    memset( opt->extra, 0, ARG_MAX);
    strzcpynn( opt->env, "PATH", ARG_MAX, 4 );

    return;
}

void
set_exist( struct options *opt, const char *arg, const int value )
{
    if ( value ) {
        opt->exist = 1;
    } else {
        opt->exist = 0;
    }
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: check exists: %d\n", arg, opt->exist );
    }
}

void
set_dir( struct options *opt, const char *arg, const int value )
{
    if ( value ) {
        opt->dir = 1;
    } else {
        opt->dir = 0;
    }
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: check if dir: %d\n", arg, opt->dir );
    }
}

void
set_file( struct options *opt, const char *arg, const int value )
{
    if ( value ) {
        opt->file = 1;
    } else {
        opt->file = 0;
    }
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: check if file: %d\n", arg, opt->file );
    }
}

void
set_before( struct options *opt, const char *arg, const int value )
{
    if ( value ) {
        opt->before = 1;
    }
    else {
        opt->before = 0;
    }
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: before %d\n", arg, opt->before );
    }
    return;
}

void
set_noenv( struct options *opt, const char *arg, const int value )
{
    if ( value ) {
        strzcpynn(opt->env, "", ARG_MAX, 1);
    }
    else {
        strzcpynn(opt->env, "PATH", ARG_MAX, 4);
    }
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: ENVNAME [%s]\n", arg, opt->env );
    }
    return;
}

void
set_env( struct options *opt, const char *arg, const char *value )
{
    if ( *value ) {
        strzcpyn(opt->env, value, ARG_MAX);
    }
    else {
        strzcpynn(opt->env, "", ARG_MAX, 1);
    }
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: ENVNAME [%s]\n", arg, opt->env );
    }
    return;
}

/****************************************************************************
 * STRING FUNCTIONS
 */

/***************************************
 * Deliberately returns end of string if seek is not found.
 */
int
strzindex( register const char seek,
            const char *str,
            int start,
            register int strlen )
{
    if ( !str ) {
        return( -1 );
    }
    if ( !strlen ) {
        return( -1 );
    }
    int p = start;
    register const char *t = (char *)(str + p);
    while ( p < strlen ) {
        if ( seek == *t ) {
            return p;
        }
        else if ( (char)0 == *t ) {
            return p;
        }
        ++p; ++t;
    }
    return -1;
}

/***************************************
 * strzlengthn -- very much like strnlen, BUT
 *    if length reaches strlen, returns zero.
 */
int
strzlengthn( const char *str, register int strlen )
{
    register int l = 0;
    register const char * p = str;
    while ( l < strlen ) {
        if ( (char)0 == *p ) {
            return l;
        }
        ++l; ++p;
    }
    return 0;
}

int
strzcpyn( char *dest, const char *src, int destlen )
{
    return strzcpynn( dest, src, destlen, destlen );
}

/***************************************
 * like strncpy, but
 * if a NUL is found in src,
 *      stops looking at src, and contineus to copy (char)0 until destlen-1
 * stops copying at destlen - 2
 * always puts a null at dest[destlen - 1]
 */
int
strzcpynn( char *dest, const char *src, int destlen, int srclen )
{
    if ( !dest ) {
        return 0;
    }
    if ( !src ) {
        return 0;
    }
    if ( !destlen ) {
        return 0;
    }
    int fzero = 0;      // found a src zero byte or end of srclen
    int p = 0;          // progress
    while ( p < ( destlen - 1 ) ) {
        if ( fzero ) {
            dest[p] = (char)0;
        }
        else if  ( p >= srclen ) {

            // Check this first, to not read past srclen
            fzero = 1;
            dest[p] = (char)0;
        }
        else if  ( (char)0 == src[p] ) {
            fzero = 1;
            dest[p] = (char)0;
        }
        else {
            dest[p] = src[p];
        }
        p++;
    }
    // ALWAYS sets destlen to NULL byte
    dest[p] = (char)0;
    return strzlengthn(dest, destlen);
}

int
strzcatn( char *dest, const char *src, int destlen )
{
    return strzcatnn( dest, src, destlen, destlen );
}

int
strzcatnn( char *dest, const char *src, int destlen, int srclen )
{
    int  dest_str_len = strzlengthn(dest, destlen);
    char *rdest = (char *)( dest + dest_str_len );
    int  rdestlen = ( destlen - dest_str_len );

    return strzcpynn( rdest, src, rdestlen, srclen );
}

int
strneqstrn( const char *seek, int seeklen, const char *str, int strlen )
{
    if ( !seeklen ) {
        return 0;
    }
    if ( seeklen != strlen ) {
        // Easy to say that mis-length strings do not match!
        return 0;
    }
    register int p = 0;   // progress
    register const char * k = seek;
    register const char * r = str;
    while ( p < seeklen ) {
        if ( *k != *r ) {
            return 0;
        }
        ++p; ++k; ++r;
    }
    return 1;
}

void
printlicense()
{
    printf("\n\
MIT License\n\
\n\
CleanPath https://gitlab.home.vollink.com/external/cleanpath/\n\
Copyright (c) 2021, Gary Allen Vollink\n\
\n\
Permission is hereby granted, free of charge, to any person obtaining a copy\n\
of this software and associated documentation files (the \"Software\"), to deal\n\
in the Software without restriction, including without limitation the rights\n\
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n\
copies of the Software, and to permit persons to whom the Software is\n\
furnished to do so, subject to the following conditions:\n\
\n\
The above copyright notice and this permission notice shall be included in all\n\
copies or substantial portions of the Software.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n\
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n\
SOFTWARE.\n\n");
}
/* EOF cleanpath.c */
