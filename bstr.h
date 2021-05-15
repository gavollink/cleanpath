#ifndef VOLLINK_BSTR_H
#define VOLLINK_BSTR_H

typedef struct {
    char *  s;
    size_t  l;
    size_t  r;
    size_t  a;
} bstr;

#define BS(x)       x->s
#define BSFIX(x)    bstr_len(x)

bstr *  new_bstr(size_t len);
void    free_bstr(bstr *str);
void    free_ALL_bstr();
        // If no NULL by limit, returns zero
int     strz_len_z(const char * src, int limit);
        // If no NULL by limit, returns limit
int     strz_len_n(const char * src, int limit);
int     strz_len(const char * src);
int     bstr_len(bstr *src);
        // Only if newlen is smaller than src->l
int     bstr_setlen(bstr *src, size_t newlen);
int     bstr_copy(bstr *dest, const bstr *src);
int     bstr_copystrz(bstr *dest, const char *src, const size_t srclimit);
int     bstr_cat(bstr *dest, const bstr *src);
int     bstr_catstrz(bstr *dest, const char *src, const size_t srclimit);

int     bstr_index(const char needle, const bstr *haystack, int start);
int     bstr_eq(const bstr *a, const bstr *b);
int     bstr_splice( bstr* victim, int from, int to, bstr* dest );

#endif
