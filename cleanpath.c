/****************************************************************************
 * cleanpath.c
 *
 * Linux/MacOS utility to clean a colon separated ENV variable.
 *
 * LICENSE: Embedded at bottom...
 */
#define CP_VERSION "1.07"

#include <stdio.h>          // printf
#include <stdlib.h>         // exit, malloc, free
// These two are ONLY used for strerror(errno) on mem allocation failure.
#include <errno.h>
#include <string.h>
// stat()
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#   include <bits/stat.h>
#endif
#if 0
// ARG_MAX
#ifdef __MACH__
    // MacOS file is here
#include <sys/syslimits.h>
#elif defined (__hpux)
    // HP/UX file /may/ be here
#include <sys/param.h>
    // or here
#include <limits.h>
#elif defined (__linux__)
#include <linux/limits.h>
    // Add More, If you have them
#else
    // Something on the internet suggested that this covers Solaris
#include <limits.h>
#endif
#endif

#include "bstr.h"

struct options {
    int     exist;
    int     file;
    int     dir;
    int     before;
    int     debug;
#if 0
    int     sizewarn;
#endif
    char    delimiter;
    bstr    *env;
    bstr    *extra;
};

bstr *  tokenwalk( struct options *opt, bstr *whole );
int     check_opt( struct options *opt, int argc, char *argv[] );
void    help(char *me);
void    usage(char *me);
void    version(char *me);
void    printlicense();
int     strneqstrn( const char *seek, int seeklen, const char *str, int strlen );
void    default_opt( struct options *opt );
void    set_exist( struct options *opt, const char *arg, const int val );
void    set_dir( struct options *opt, const char *arg, const int val );
void    set_file( struct options *opt, const char *arg, const int val );
void    set_before( struct options *opt, const char *arg, const int val );
void    set_noenv( struct options *opt, const char *arg, const int val );
void    set_env( struct options *opt, const char *arg, const char *val );
char *  elim_mult( char *str, int strlen, struct options *opt );
bstr *  dedupe( struct options *opt, bstr *buffer );
void    myexit(int status);

#define S_I_ALL (S_IFMT|S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)

int
main( int argc, char *argv[] )
{
    int memblk = 0;
    bstr* holdenv;
    char* origenv;
    struct options opts;

    // init opt structure with defaults
    default_opt( &opts );
    // Set options
    check_opt( &opts, argc, argv );

    origenv = getenv(opts.env->s);
    if ((origenv) && (*origenv)) {
        memblk = strz_len(origenv);
    }
    holdenv = new_bstr( memblk );
    if ( holdenv == NULL ) { exit(5); }

    if ( opts.debug ) {
        if ( !origenv ) {
            fprintf(stderr, "Pull ENVNAME, %s, is empty\n", opts.env->s);
        } else {
            fprintf(stderr, "Pull ENVNAME, %s, \"%s\"\n", opts.env->s, origenv);
        }
    }

    if ( opts.before ) {
        // Concat any command-line extras into holdenv
        bstr_cat(holdenv, opts.extra);
        // Add a delimiter to holdenv
        bstr_catstrz(holdenv, &opts.delimiter, 1);
        // Cat requested ENV VAR onto holdenv
        bstr_catstrz(holdenv, origenv, holdenv->a);
    }
    else {
        // Copy requested ENV VAR into holdenv
        bstr_catstrz(holdenv, origenv, holdenv->a);
        // Add a delimiter to holdenv
        bstr_catstrz(holdenv, &opts.delimiter, 1);
        // Concat any command-line extras into holdenv
        bstr_cat(holdenv, opts.extra);
    }

    if ( opts.debug ) {
        fprintf(stderr, "Concat ENV and ENVADD => \"%s\"\n", holdenv->s);
    }

    // Eliminate multiple :
    elim_mult( holdenv->s, holdenv->a, &opts );
    if ( opts.debug ) {
        fprintf(stderr, "Remove redundant delimiters \"%s\"\n", holdenv->s);
    }

    tokenwalk( &opts, holdenv );

    while ( holdenv->s[ holdenv->l - 1 ] == opts.delimiter ) {
        holdenv->s[ --holdenv->l ] = (char)0;
    }

    printf( "%s\n", BS(holdenv) );
    myexit(0);
}

int
dedupe_in( struct options *opt, bstr *whole, bstr *test, int start )
{
    int    inn_s = start;
    int    inn_n = start;
    bstr *itoken = new_bstr(whole->l);

    while ( inn_n < bstr_len(whole) ) {
        inn_n = bstr_index(opt->delimiter, whole, inn_s);
        if ( ( 0 == inn_s ) && ( inn_n >= whole->l ) ) {
            if ( opt->debug ) {
                fprintf(stderr, "dedupe(): only one token");
            }
            break;
        }
        bstr_copystrz( itoken, (whole->s + inn_s), inn_n - inn_s );
#ifdef DEBUG
        if ( 2 <= opt->debug ) {
            fprintf( stderr, "DUP EVAL (%d) [%s] to [%s]\n",
                inn_s, BS(itoken), BS(test));
        }
#endif
        if ( !bstr_eq( itoken, test ) ) {
            if ( opt->debug ) {
                fprintf( stderr, "duplicate token: [%s] (removing)\n",
                    BS(itoken) );
            }
            bstr_splice( whole, inn_s, inn_n + 1, NULL );
            if ( opt->debug ) {
                fprintf( stderr, "duplicate token: [%s] removed leaving [%s]\n",
                    BS(itoken), BS(whole) );
            }
            inn_s = inn_n = start;
            continue;
        }
        inn_s = inn_n + 1;
    }
    return(whole->l);
}

int
token_check( struct options *opt, bstr *token )
{
    int modefail = 0;
    struct stat statbuf;
    int statret;
    if ( opt->exist || opt->file || opt->dir ) {
        statret = stat( BS(token), &statbuf );
        if ( -1 == statret ) {
            if ( opt->debug ) {
                fprintf( stderr, "token_check(): Not exists: \"%s\"\n",
                    BS(token) );
            }
            modefail = 7;
        }
        /* file and dir checks below here, everything else, add above */
        else if ( opt->file && opt->dir ) {
            /* If BOTH are set, BOTH of these have to fail */
            if (   ( S_IFDIR != ( statbuf.st_mode & S_IFMT ) )
                && ( S_IFREG != ( statbuf.st_mode & S_IFMT ) ) )
            {
                if ( opt->debug ) {
                    fprintf( stderr, "token_check(): Not a regular file or dir: \"%s\"\n",
                        BS(token) );
                }
                modefail = 3;
            }
        }
        else if ( ( opt->file )
            && ( S_IFREG != ( statbuf.st_mode & S_IFMT ) ) )
        {
            if ( opt->debug ) {
                fprintf( stderr, "token_check(): Not a regular file: \"%s\"\n", BS(token) );
            }
            modefail = 2;
        }
        else if ( ( opt->dir )
            && ( S_IFDIR != ( statbuf.st_mode & S_IFMT ) ) )
        {
            if ( opt->debug ) {
                fprintf( stderr, "token_check(): Not a directory: \"%s\"\n", BS(token) );
            }
            modefail = 1;
        }
    }
    return (modefail);
}

bstr *
tokenwalk( struct options *opt, bstr *whole )
{
    bstr *otoken = new_bstr(whole->l);
    int    out_s = 0;
    int    out_n = 0;
    while ( out_n < bstr_len(whole) ) {
        out_n = bstr_index(opt->delimiter, whole, out_s);
        bstr_copystrz( otoken, (whole->s + out_s), out_n - out_s );
        if ( opt->debug ) {
            fprintf( stderr, "EVALUATE (%d) [%s] of [%s]\n",
                out_s, BS(otoken), BS(whole) );
        }
        dedupe_in( opt, whole, otoken, out_n + 1 );
        if ( token_check( opt, otoken ) ) {
            bstr_splice( whole, out_s, out_n + 1, NULL );
            if ( opt->debug ) {
                fprintf( stderr, "tokenwalk(): Removed (%d)[%s] from [%s]\n",
                    out_s, BS(otoken), BS(whole) );
            }
            /* Chopped out current token, continue with new current token */
            out_n = out_s;
            continue;
        }
        out_s = out_n + 1;
    }
    return whole;
}

int
check_opt( struct options *opt, int argc, char *argv[] )
{
    int argcx;
    int askhelp    = 0;
    int asklicense = 0;
    int askversion = 0;
    int haveenv    = 0;
    int argFEatsArg = 0;

    // Read command line options...
    for ( argcx = 1; argcx < argc; argcx++ ) {

        // Long Options, anything that starts with --
        if ( strneqstrn( "--", 2, argv[argcx], 2 ) ) {
#ifdef DEBUG
            if ( 2 <= opt->debug ) {
                fprintf( stderr, "CMDLINE %d [%s]\n", argcx, argv[argcx] );
            }
#endif
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
                    myexit(2);
                }
                haveenv = 1;
            }
#if 0
            else if ( strneqstrn( "--nosizelimit", strlen("--nosizelimit"),
                        argv[argcx], strlen(argv[argcx]) ) )
            {
                opt->sizewarn = 0;
            }
#endif
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
            else if ( strneqstrn( "--version", strlen("--version"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                askversion = 1;
            }
#ifdef DEBUG
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
#endif
            else if ( strneqstrn( "--debug", strlen("--debug"),
                argv[argcx], strlen(argv[argcx]) ) )
            {
                opt->debug = 1;
            }
            else if ( strneqstrn( "--delimiter", strlen("--delimiter"),
                argv[argcx], strlen("--delimiter") ) )
            {
                int getnow = 0;
                if ( strlen(argv[argcx]) == strlen("--delimiter") ) {
                    getnow = 1;
                }
                else if ( strlen(argv[argcx]) > 2+strlen("--delimiter") ) {
                    fprintf( stderr,
                            "Unrecognized option '%s'\n",
                            argv[argcx] );
                    usage(argv[0]);
                    myexit(2);
                }
                else if ( strneqstrn( "--delimiter=", strlen("--delimiter="),
                            argv[argcx], strlen("--delimiter=") ) )
                {
                    if ( strlen(argv[argcx]) == 1 + strlen("--delimiter=") )
                    {
                        getnow = 0;
                        opt->delimiter = argv[argcx][strlen(argv[argcx])-1];
                    } else {
                        getnow = 1;
                    }
                }
                else {
                    fprintf( stderr,
                            "Unrecognized option '%s'\n",
                            argv[argcx] );
                    usage(argv[0]);
                    myexit(2);
                }
                if ( getnow ) {
                    if ( ( argcx + 1 < argc )
                        && ( 1 == strlen(argv[argcx+1]) ) )
                    {
                        argcx++;
                        opt->delimiter = argv[argcx][0];
                    }
                    else {
                        fprintf( stderr,
                                "Used option '%s', but no delimiter.\n",
                                argv[argcx] );
                        usage(argv[0]);
                        myexit(2);
                    }
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
                myexit(2);
            }
        }
        else if ( strneqstrn( "-", 1, argv[argcx], 1 ) ) {
#ifdef DEBUG
            if ( 2 <= opt->debug ) {
                fprintf( stderr, "CMDLINE %d [%s]\n", argcx, argv[argcx] );
            }
#endif
            if ( 1 >= strlen( argv[argcx] ) ) {
                fprintf( stderr, "Unrecognized option '%s'\n", argv[argcx] );
                usage(argv[0]);
                myexit(2);
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
#if 0
                else if ( 'S' == argv[argcx][cx] ) {
                    opt->sizewarn = 0;
                }
#endif
                else if ( 'F' == argv[argcx][cx] ) {
                    needf = 1;
                }
                else if ( 'X' == argv[argcx][cx] ) {
                    set_noenv( opt, "-X", 1 );
                    haveenv = 1;
                }
                else if ( 'v' == argv[argcx][cx] ) {
                    opt->debug = 1;
                }
                else if ( 'V' == argv[argcx][cx] ) {
                    askversion = 1;
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
                    myexit(2);
                }
            }
            if ( needf ) {
                if ( argcx + 1 >= argc )
                {
                    fprintf( stderr,
                        "No delimiter given for '-F'\n" );
                    usage(argv[0]);
                    myexit(2);
                }
                else if ( 1 != strlen(argv[argcx+1]) ) {
                    fprintf( stderr,
                        "Delimiter given for '-F', needs length 1 (got '%s')\n",
                        argv[argcx+1] );
                    usage(argv[0]);
                    myexit(2);
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
            else if ( strneqstrn( "--delimiter=", strlen("--delimiter="),
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
#ifdef DEBUG
                if ( 2 <= opt->debug ) {
                    fprintf( stderr, "CMDLINE %d [%s] as ENVNAME\n",
                        argcx, argv[argcx] );
                }
#endif
                set_env( opt, "ENVNAME(bareword)", argv[argcx] );
                haveenv = 1;
            }
            else {
#ifdef DEBUG
                if ( 2 <= opt->debug ) {
                    fprintf( stderr, "CMDLINE %d [%s] as ENVADD\n",
                        argcx, argv[argcx] );
                }
#endif
                bstr_catstrz( opt->extra, &opt->delimiter, 1 );
                size_t arglen = strz_len(argv[argcx]);
                bstr_catstrz( opt->extra, argv[argcx], arglen+1 );
            }
        }
    }

    if ( opt->before && ( ! *opt->env->s ) ) {
        fprintf( stderr, "%s\n", "WARN: --before meaningless with --noenv" );
    }

    if ( opt->debug ) {
        fprintf( stderr, "     --exists: %d\n",
            (opt->exist|opt->file|opt->dir) );
        fprintf( stderr, " --checkpaths: %d\n", opt->dir );
        fprintf( stderr, " --checkfiles: %d\n", opt->file );
        fprintf( stderr, "  --delimiter:'%c'\n", opt->delimiter );
        fprintf( stderr, "     --before: %d\n", opt->before );
#if 0
        fprintf( stderr, "--nosizelimit: %d\n", !opt->sizewarn );
#endif
        fprintf( stderr, "      ENVNAME: %s\n",
                *opt->env->s?opt->env->s:"\t(none)" );
        fprintf( stderr, "       ENVADD: %s\n", opt->extra->s );
    }
    if ( askhelp | asklicense | askversion ) {
        if ( askhelp ) {
            help(argv[0]);
        }
        if ( asklicense ) {
            printlicense();
        }
        if ( askversion ) {
            version(argv[0]);
        }
        myexit(0);
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
version(char *me)
{
    if ( me ) {
        printf( "%s Version: %s\n", me, CP_VERSION );
    } else {
        printf( "Version: %s\n", CP_VERSION );
    }
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
        "--before | -b" );
    printf( "\t\t%s\n",
                "Put ENVADD tokens before ENV in output." );
    printf( "\t%s\n",
        "--exists | -e" );
    printf( "\t\t%s\n",
                "Verify that each token exists in the filespace." );
    printf( "\t\t%s\n",
                "Implied by --checkfiles and --checkpaths" );
    printf( "\t%s\n",
        "--checkfiles | -f" );
    printf( "\t\t%s\n",
                "Verify that each token is a regular file." );
    printf( "\t%s\n",
        "--checkpaths | -P" );
    printf( "\t\t%s\n",
                "Verify that each token as a valid directory." );
    printf( "\t%s\n",
        "--delimiter | -F" );
    printf( "\t\t%s\n",
                "Single character delimiter of tokens." );
    printf( "\t\t%s\n",
                "Default is colon (:)" );
    printf( "\t%s\n",
        "--env ENVNAME" );
    printf( "\t\t%s\n",
                "Explicit setting of ENVNAME." );
    printf( "\t%s\n",
        "--noenv | -X" );
    printf( "\t\t%s\n",
                "Do not pull contents of any environment variable" );
    printf( "\n" );
    printf( "\t%s\n",
        "ENVNAME" );
    printf( "\t\t%s\n",
                "Name of environment variable to evaluate." );
    printf( "\t\t%s\n",
                "If supplied must come before ENVADD and --." );
    printf( "\t\t%s\n",
                "Disable with --noenv | -X." );
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
#if 0
    printf( "\t%s\n",
        "--nosizelimit | -S" );
    printf( "\t\t%s\n",
                "Do not print warning and shorten if processed string" );
    printf( "\t\t%s\n",
                "+ ENVNAME + '=' is too large for a command line (ARG_MAX)." );
    printf( "\t\t%s\n",
                "Useful as pipe source." );
#endif
    printf( "\t%s\n",
        "--help | -h" );
    printf( "\t\t%s\n",
                "This help text." );
    printf( "\t%s\n",
        "--license" );
    printf( "\t\t%s\n",
                "Show License." );
    printf( "\t%s\n",
        "--debug | -v" );
    printf( "\t\t%s\n",
                "Enable debugging output." );
#if 0
    /* Deliberately hiding --vdebug, even if it's enabled */
#ifdef DEBUG
    printf( "\t%s\n",
        "--vdebug" );
    printf( "\t\t%s\n",
                "Enable dev debugging output." );
#endif
#endif
    printf( "\n" );
    version(NULL);
#if 0
    printf( "\t(System ARG_MAX: %d)\n", ARG_MAX );
#endif
    printf( "\n" );
    printf( "Copyright (c) 2021, 2023 Gary Allen Vollink -- MIT License\n" );
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

    for( ; (s - str) <= (strz_len_z(str, strlen)+1); s++ ) {
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
    opt->exist     = 0;
    opt->file      = 0;
    opt->dir       = 0;
    opt->before    = 0;
    opt->debug     = 0;
#if 0
    opt->sizewarn  = 1;
#endif
    opt->delimiter = ':';
    opt->extra     = new_bstr(0);
    opt->env       = new_bstr(4);

    if ( opt->extra == NULL ) { myexit(5); }
    if ( opt->env   == NULL ) { myexit(5); }

    bstr_catstrz( opt->env, "PATH", 4 );

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
#ifdef DEBUG
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: check exists: %d\n", arg, opt->exist );
    }
#endif
}

void
set_dir( struct options *opt, const char *arg, const int value )
{
    if ( value ) {
        opt->dir = 1;
    } else {
        opt->dir = 0;
    }
#ifdef DEBUG
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: check if dir: %d\n", arg, opt->dir );
    }
#endif
}

void
set_file( struct options *opt, const char *arg, const int value )
{
    if ( value ) {
        opt->file = 1;
    } else {
        opt->file = 0;
    }
#ifdef DEBUG
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: check if file: %d\n", arg, opt->file );
    }
#endif
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
#ifdef DEBUG
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: before %d\n", arg, opt->before );
    }
#endif
    return;
}

void
set_noenv( struct options *opt, const char *arg, const int value )
{
    if ( value ) {
        bstr_copystrz(opt->env, "", 1);
    }
    else {
        bstr_copystrz(opt->env, "PATH", 4);
    }
#ifdef DEBUG
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: ENVNAME [%s]\n", arg, opt->env->s );
    }
#endif
    return;
}

void
set_env( struct options *opt, const char *arg, const char *value )
{
    if ( *value ) {
        size_t arglen = strz_len(value);
        bstr_copystrz(opt->env, value, arglen+1);
    }
    else {
        bstr_copystrz(opt->env, "", 1);
    }
#ifdef DEBUG
    if ( 2 <= opt->debug ) {
        fprintf( stderr, "    %s: ENVNAME [%s]\n", arg, opt->env->s );
    }
#endif
    return;
}

/****************************************************************************
 * STRING FUNCTIONS
 */
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
myexit(int v)
{
    free_ALL_bstr();
    exit(v);
}

void
printlicense()
{
    printf("\n\
MIT License\n\
\n\
CleanPath https://gitlab.home.vollink.com/external/cleanpath/\n\
Copyright (c) 2021, 2023 Gary Allen Vollink\n\
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
