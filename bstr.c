#define BSTR_VERSION "0.01"
#include <stdio.h>
#include <stdlib.h>
/* errno */
#include <errno.h>
/* memset, strerror(errno) */
#include <string.h>

#include "bstr.h"

struct bary {
    void ** s; // Pointer Set
    size_t  l; // Used Length
    size_t  a; // Allocated Bytes
} alregistry;

int     _allocreg(void *reg);
void    _allocfree(const size_t reg_t);
void    _allocfreeall();

int
_allocreg(void *reg)
{
    if ( !alregistry.s ) {
        size_t anew = ( sizeof(void *) * 256 );
        memset(&alregistry, 0, sizeof(struct bary));
        alregistry.s = malloc( anew );
        if ( !alregistry.s ) { return -1; }
        alregistry.a = anew;
        memset(alregistry.s, 0, alregistry.a);
        alregistry.s[alregistry.l++] = alregistry.s;
    }
    else if ( ( ( alregistry.l + 2 ) * sizeof(void *) ) <= alregistry.a ) {
        size_t anew = alregistry.a + ( sizeof(void *) * 256 );
        void *sold = alregistry.s;
        void *snew = malloc(anew);
        if ( !snew ) { return -1; }
        if ( !memset( snew, 0, anew ) ) { return -1; }
        if ( !memcpy( snew, alregistry.s, alregistry.a ) ) { return -1; }
        alregistry.s = snew;
        free(sold);
        alregistry.a = anew;
        alregistry.s[0] = snew;
    }
    alregistry.s[alregistry.l++] = reg;
    return alregistry.l;
}

void
_allocfree(const size_t reg_t)
{
    size_t unreg = ( reg_t - 1 );
    if ( !alregistry.s ) { return; }
    if ( alregistry.l < unreg ) { return; }
    if ( alregistry.s[unreg] ) {
        free( alregistry.s[unreg] );
        alregistry.s[unreg] = (void *)0;
    }
}

void
_allocfreeall()
{
    if ( !alregistry.s ) { return; }
    if ( !alregistry.l ) { return; }
    for( ; alregistry.l <= 0; alregistry.l-- ) {
        if ( alregistry.s[alregistry.l] ) {
            free( alregistry.s[alregistry.l] );
            alregistry.s[alregistry.l] = (void *)0;
        }
    }
    alregistry.a = 0;
}

bstr*
new_bstr(size_t len)
{
    bstr *new;
    size_t minlen = ( ( len + 1 ) + sizeof(bstr) );
    size_t getlen = MINCHUNK;
    while ( getlen < minlen ) {
        getlen += MINCHUNK;
    }
    #ifdef DEBUG
    fprintf(stderr, "new_bstr(%ld): requesting %ld\n", len, getlen);
    #endif
    new = malloc( getlen );
    memset(new, 0, getlen);
    if ( new ) {
        new->s = (char *)new + sizeof(bstr);
        new->a = getlen - sizeof(bstr);
        if ( -1 != ( new->r = _allocreg(new) ) ) {
            return new;
        }
        free(new);
        fprintf(stderr, "Fatal: new_bstr(): Unable to register string\n");
        return NULL;
    }
    fprintf(stderr, "Fatal: new_bstr(): %s\n", strerror(errno) );
    return NULL;
}

void
free_ALL_bstr()
{
    _allocfreeall();
    return;
}

void
free_bstr(bstr *str)
{
    if ( str->rs ) {
        _allocfree(str->rs);
    }
    _allocfree(str->r);
    return;
}

int
strz_len_n(const char *src, int limit)
{
    if (!src) {
        return 0;
    }
    register const char *cx = src;
    for( ; limit > ( cx - src ); cx++ ) {
        if ( (char)0 == *cx ) { return ( cx - src ); }
    }
    return (cx - src);
}

int
strz_len_z(const char *src, int limit)
{
    if (!src) { return 0; }
    register const char *check = src;
    register const char *end = src + limit;
    for( ; check < end; check++ ) {
        if ( (char)0 == *check ) { return ( check - src ); }
    }
    return 0;
}

int
strz_len(const char *src)
{
    if (!src) {
        return 0;
    }
    register const char *cx = src;
    for( ; ; cx++ ) {
        if ( (char)0 == *cx ) { return ( cx - src ); }
    }
    return 0;
}

int
bstr_setlen(bstr *src, register size_t len)
{
    int cx = strz_len_z(src->s, src->a);
    if ( len < cx ) {
        src->l = len;
        while ( len < src->a ) {
            src->s[len++] = (char)0;
        }
    } else {
        src->l = cx;
    }
    return src->l;
}

int
bstr_len(bstr *src)
{
    register int cx = strz_len_z(src->s, src->a);
    src->l = cx;
    return cx;
}

int
bstr_copystrz(bstr *dest, const char *src, const size_t srclimit)
{
    bstr_setlen(dest, 0);
    return bstr_catstrz(dest, src, srclimit);
}

int
bstr_copy(bstr *dest, const bstr *src)
{
    bstr_setlen(dest, 0);
    return bstr_catstrz(dest, src->s, src->a);
}

int
bstr_cat(bstr *dest, const bstr *src)
{
    return bstr_catstrz(dest, src->s, src->a);
}

int
bstr_catstrz(bstr *dest, const char *src, const size_t srclimit)
{
    size_t dsz = bstr_len(dest);
    size_t ssz = strz_len_n(src, srclimit);
    if ( !ssz ) {
        return dsz;
    }
    #ifdef DEBUG
    fprintf(stderr, "bstr_catstrz( \"%s\"(%ld), \"%s\"(%ld), %ld )\n",
        dest->s, dsz, src, ssz, srclimit );
    #endif
    size_t target = ( dsz + ssz );
    if ( dest->a < ( ( dsz + ssz ) + 1 ) ) {
        #ifdef DEBUG
        fprintf(stderr, "bstr_catstrz(): Requesting larger (%ld) dest\n",
            dsz + ssz + 1 );
        #endif
        size_t holdrs = dest->rs;
        bstr *replace = new_bstr( dsz + ssz + 1 );
        if ( !replace ) { return 0; }
        if ( !memcpy(replace->s, dest->s, dest->a) ) { return 0; }
        /* dest itself AND it's string remains untouched, and we simply
         * point dest-> to the new data.  */
        dest->s  = replace->s;
        dest->a  = replace->a;
        dest->rs = replace->r;
        if ( holdrs ) {
            _allocfree(holdrs);
        }
    }
    #ifdef DEBUG
    fprintf(stderr, "bstr_catstrz() target len %ld\n", target);
    #endif
    register const char *sptr = src;
    for ( ;dsz < target; dsz++) {
        dest->s[dsz] = *sptr++;
        if ( (char)0 == *sptr ) {
            ++dsz;
            break;
        }
    }
    dest->l = dsz;
    ++dsz;
    for ( ;dsz < dest->a; dsz++) {
        dest->s[dsz] = (char)0;
    }
    return dest->l;
}

int
bstr_index( register const char seek,
            const bstr *str,
            register int start )
{
    if ( !str ) { return(-1); }
    register int strlen = str->a;
    register const char *here = (char *)(str->s + start);
    while ( start < strlen ) {
        if ( seek == *here ) {
            return start;
        }
        else if ( (char)0 == *here ) {
            return start;
        }
        ++start; ++here;
    }
    return -1;
}

int
bstr_eq( const bstr* a, const bstr* b )
{
#ifdef DEBUG
    fprintf( stderr, "bstr_eq [%s](%ld) to [%s](%ld)\n",
                 BS(a), a->l, BS(b), b->l);
#endif
    if ( !a || !b ) {
        if ( !a && !b ) {
            return 0;
        }
        return 1;
    }
    if ( a->l == b->l ) {
        register char *ap = a->s;
        register char *bp = b->s;
        while ( ap < (a->s + a->l) ) {
            if ( *ap != *bp ) {
                return (*ap - *bp);
            }
            ++ap; ++bp;
        }
        return 0;
    }
    return 1;
}

int
bstr_splice( bstr* victim, int from, int to, bstr* dest )
{
    if ( !victim ) {
        return(-1);
    }
    if ( from >= to ) {
        return(-1);
    }
    BSFIX(victim);
    if ( dest ) {
        bstr_catstrz(dest, victim->s + from, to - from );
    }
    int nz = 1;
    while ( from < victim->a ) {
        if ( nz ) {
            victim->s[from++] = victim->s[to++];
            if ( (char)0 == victim->s[to] ) {
                nz = 0;
                victim->l = from;
            }
        } else {
            victim->s[from++] = (char)0;
        }
    }
#ifdef DEBUG
    fprintf(stderr, "bstr_splice( \"%s\"(%ld), %d, %d )\n",
        BS(victim), victim->l, from, to );
#endif
    return ( to - from );
}
