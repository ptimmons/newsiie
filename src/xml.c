/*
 * The streaming XML scanner. See xml.h.
 *
 * Bytes are handled in two ways. Runs of text, CDATA, comments and quoted
 * attribute values go through scan(), which finds the next byte that
 * matters at assembly speed and copies (or skips) everything before it in
 * one go. Tag names and the bytes around them go through the per-byte
 * state machine below. Feeds are mostly text, so most bytes take the
 * first path.
 */
#include <string.h>
#include "xml.h"
#include "scan.h"

char    *xml_cap;
uint16_t xml_cap_len;
uint16_t xml_cap_max;
char     xml_href[512];
char     xml_rel[16];
uint8_t  xml_latin1;
uint8_t  xml_stop;
void   (*xml_tick)(void);

enum {
    S_TEXT,         /* between tags */
    S_LT,           /* just after '<' */
    S_NAME,         /* an open tag's name */
    S_ATTRS,        /* inside an open tag, between attributes */
    S_ANAME,        /* an attribute's name (links only) */
    S_AEQ,          /* after an attribute name, before its quote */
    S_AVAL,         /* inside a quoted value */
    S_ENDNAME,      /* a close tag's name */
    S_ENDREST,      /* after a close tag's name, up to '>' */
    S_BANG,         /* after "<!" */
    S_CDATA,        /* inside <![CDATA[ ... */
    S_COMMENT,      /* inside <!-- ... */
    S_DECL,         /* <!DOCTYPE and other declarations */
    S_DECLSUB,      /* a DOCTYPE's [internal subset] */
    S_PI,           /* <? ... ?> */
    S_ENT           /* an entity in captured text */
};

#define NAME_MAX    31
#define ENT_MAX     10
#define PI_MAX      80

static uint8_t st;
static uint8_t depth;
static char    name[NAME_MAX + 1];
static uint8_t nlen;
static uint8_t elem;            /* id of the tag being read */
static uint8_t slash;           /* '/' seen: a self-closing tag */
static char    aname[8];        /* attribute name, links only */
static uint8_t alen;
static char   *aval;            /* where a wanted value goes, or NULL */
static uint16_t avlen, avmax;   /* hrefs run to 511 */
static uint8_t quote;           /* SC_DQ or SC_SQ while in a value */
static char    ent[ENT_MAX + 1];
static uint8_t elen;
static uint8_t run;             /* ']' or '-' seen in a row, for ]]> and --> */
static char    pi[PI_MAX + 1];
static uint8_t pilen;

static const char *const names[] = {
    "rss", "channel", "item", "feed", "entry", "rdf:RDF", "html",
    "title", "link", "description", "summary", "content",
    "content:encoded", "pubDate", "published", "updated", "dc:date",
    "guid", "id",
    "p", "br", "div", "li"
};
#define NNAMES  (sizeof names / sizeof names[0])
static uint8_t name_len[NNAMES];

void xml_reset(void)
{
    uint8_t i;

    st = S_TEXT;
    depth = 0;
    xml_cap = NULL;
    xml_cap_len = 0;
    xml_latin1 = 0;
    xml_stop = 0;
    for (i = 0; i < NNAMES; ++i) {
        name_len[i] = (uint8_t)strlen(names[i]);
    }
}

static uint8_t lookup(void)
{
    const char *s = name;
    uint8_t n = nlen, i;

    if (n > 5 && !memcmp(s, "atom:", 5)) {
        s += 5;
        n -= 5;
    }
    for (i = 0; i < NNAMES; ++i) {
        if (name_len[i] == n && names[i][0] == s[0] && !memcmp(names[i], s, n)) {
            return (uint8_t)(i + 1);
        }
    }
    return XE_OTHER;
}

/* ---- captured text ---- */

static void put_byte(uint8_t c)
{
    if (xml_cap_len < xml_cap_max) {
        xml_cap[xml_cap_len++] = (char)c;
    }
}

/* A code point as UTF-8 (clean.c reads UTF-8 only). Code points are 16
 * bits throughout: 32-bit arithmetic costs a lot of 6502 code, and nothing
 * past U+FFFF (emoji, mostly) folds to anything but '?' anyway, so those
 * arrive here as U+FFFD. */
static void put_cp(uint16_t cp)
{
    if (cp < 0x80) {
        put_byte((uint8_t)cp);
    } else if (cp < 0x800) {
        put_byte((uint8_t)(0xC0 | (cp >> 6)));
        put_byte((uint8_t)(0x80 | (cp & 0x3F)));
    } else {
        put_byte((uint8_t)(0xE0 | (cp >> 12)));
        put_byte((uint8_t)(0x80 | ((cp >> 6) & 0x3F)));
        put_byte((uint8_t)(0x80 | (cp & 0x3F)));
    }
}

static void put_run(const uint8_t *p, uint16_t k)
{
    uint16_t room;

    if (xml_latin1) {
        /* Latin-1 bytes become UTF-8, so clean.c sees one encoding. */
        while (k--) {
            put_cp(*p++);
        }
        return;
    }
    room = (uint16_t)(xml_cap_max - xml_cap_len);
    if (k > room) {
        k = room;
    }
    memcpy(xml_cap + xml_cap_len, p, k);
    xml_cap_len += k;
}

/* The code point of an entity name (no '&' or ';'), or 0 if unknown.
 * A numeric one past U+FFFF is U+FFFD. */
static uint16_t entity(const char *e)
{
    uint16_t v = 0;
    uint8_t c, big = 0;

    if (e[0] == '#') {
        if (e[1] == 'x' || e[1] == 'X') {
            for (e += 2; (c = (uint8_t)*e) != 0; ++e) {
                if (v > 0x0FFF) big = 1;
                v <<= 4;
                if (c >= '0' && c <= '9') v |= (uint8_t)(c - '0');
                else if ((c | 0x20) >= 'a' && (c | 0x20) <= 'f') v |= (uint8_t)((c | 0x20) - 'a' + 10);
                else return 0;
            }
        } else {
            for (++e; (c = (uint8_t)*e) != 0; ++e) {
                if (c < '0' || c > '9') return 0;
                if (v > 6553) big = 1;
                v = (uint16_t)(v * 10 + (uint8_t)(c - '0'));
            }
        }
        return big ? 0xFFFD : v;
    }
    if (!strcmp(e, "amp"))  return '&';
    if (!strcmp(e, "lt"))   return '<';
    if (!strcmp(e, "gt"))   return '>';
    if (!strcmp(e, "quot")) return '"';
    if (!strcmp(e, "apos")) return '\'';
    return 0;
}

/* An entity that will not decode is kept as written: the HTML pass in
 * clean.c knows the HTML names (&nbsp; and friends) that XML does not. */
static void put_entity(void)
{
    uint16_t cp;

    ent[elen] = 0;
    cp = elen ? entity(ent) : 0;
    if (cp) {
        put_cp(cp);
        return;
    }
    put_byte('&');
    put_run((const uint8_t *)ent, elen);
    put_byte(';');
}

/* Decode the five XML entities and numeric ones in an attribute value, in
 * place. Values are short and only links carry one we keep. */
static void decode_attr(char *s)
{
    char *w = s, *e;
    uint16_t cp;

    while (*s) {
        if (*s == '&' && (e = strchr(s, ';')) != NULL && e - s <= ENT_MAX) {
            *e = 0;
            cp = entity(s + 1);
            if (cp && cp < 0x80) {
                *w++ = (char)cp;
                s = e + 1;
                continue;
            }
            *e = ';';
        }
        *w++ = *s++;
    }
    *w = 0;
}

/* ---- tags ---- */

static void open_tag(void)
{
    if (elem == XE_LINK) {
        decode_attr(xml_href);
    }
    ++depth;
    xml_open(elem, depth);
    if (slash) {
        xml_close(elem, depth);
        --depth;
    }
    st = S_TEXT;
}

static void close_tag(void)
{
    elem = lookup();
    xml_close(elem, depth);
    if (depth) {
        --depth;
    }
}

static uint8_t is_space(uint8_t c)
{
    return (uint8_t)(c == ' ' || c == '\n' || c == '\r' || c == '\t');
}

/* <?xml ... encoding="..."?> */
static void end_pi(void)
{
    pi[pilen] = 0;
    if (!memcmp(pi, "xml", 3) &&
        (strstr(pi, "ISO-8859-1") || strstr(pi, "iso-8859-1") ||
         strstr(pi, "windows-1252") || strstr(pi, "Windows-1252") ||
         strstr(pi, "latin1"))) {
        xml_latin1 = 1;
    }
}

void xml_feed(const uint8_t *p, uint16_t n)
{
    uint16_t k;
    uint8_t c;

    while (n && !xml_stop) {
        switch (st) {

        case S_TEXT:
            if (xml_cap) {
                k = scan(p, n, SC_LT | SC_AMP);
                put_run(p, k);
            } else {
                k = scan(p, n, SC_LT);
            }
            p += k;
            n -= k;
            if (!n) {
                return;
            }
            if (*p == '&') {
                elen = 0;
                st = S_ENT;
            } else {
                st = S_LT;
                if (xml_tick) {
                    xml_tick();
                }
            }
            ++p;
            --n;
            break;

        case S_ENT:
            c = *p;
            if (c == ';') {
                put_entity();
                st = S_TEXT;
                ++p;
                --n;
            } else if (elen < ENT_MAX && !is_space(c) && c != '<' && c != '&') {
                ent[elen++] = (char)c;
                ++p;
                --n;
            } else {
                /* Not an entity after all: keep what was there and look at
                 * this byte again as text. */
                put_byte('&');
                put_run((const uint8_t *)ent, elen);
                st = S_TEXT;
            }
            break;

        case S_LT:
            c = *p++;
            --n;
            if (c == '/') {
                nlen = 0;
                st = S_ENDNAME;
            } else if (c == '!') {
                pilen = 0;
                st = S_BANG;
            } else if (c == '?') {
                pilen = 0;
                st = S_PI;
            } else {
                name[0] = (char)c;
                nlen = 1;
                slash = 0;
                st = S_NAME;
            }
            break;

        case S_NAME:
            c = *p++;
            --n;
            if (c == '>' || c == '/' || is_space(c)) {
                name[nlen] = 0;
                elem = lookup();
                if (elem == XE_LINK) {
                    xml_href[0] = 0;
                    xml_rel[0] = 0;
                }
                if (c == '>') {
                    open_tag();
                } else {
                    slash = (uint8_t)(c == '/');
                    st = S_ATTRS;
                }
            } else if (nlen < NAME_MAX) {
                name[nlen++] = (char)c;
            }
            break;

        case S_ATTRS:
            if (elem != XE_LINK) {
                /* Nothing wanted: run to the next '>' or quote. */
                k = scan(p, n, SC_GT | SC_DQ | SC_SQ);
                if (k) {
                    /* A '/' right before '>' makes it self-closing. */
                    c = p[k - 1];
                    slash = (uint8_t)(c == '/');
                }
                p += k;
                n -= k;
                if (!n) {
                    return;
                }
            }
            c = *p++;
            --n;
            if (c == '>') {
                open_tag();
            } else if (c == '/') {
                slash = 1;
            } else if (c == '"' || c == '\'') {
                quote = c == '"' ? SC_DQ : SC_SQ;
                aval = NULL;
                slash = 0;
                st = S_AVAL;
            } else if (!is_space(c)) {
                aname[0] = (char)c;
                alen = 1;
                slash = 0;
                st = S_ANAME;
            }
            break;

        case S_ANAME:
            c = *p++;
            --n;
            if (c == '=' || is_space(c)) {
                aname[alen] = 0;
                st = S_AEQ;
            } else if (c == '>') {
                open_tag();
            } else if (alen < sizeof aname - 1) {
                aname[alen++] = (char)c;
            }
            break;

        case S_AEQ:
            c = *p++;
            --n;
            if (c == '"' || c == '\'') {
                quote = c == '"' ? SC_DQ : SC_SQ;
                if (!strcmp(aname, "href")) {
                    aval = xml_href;
                    avmax = sizeof xml_href - 1;
                } else if (!strcmp(aname, "rel")) {
                    aval = xml_rel;
                    avmax = sizeof xml_rel - 1;
                } else {
                    aval = NULL;
                }
                avlen = 0;
                st = S_AVAL;
            } else if (c == '>') {
                open_tag();
            }
            break;

        case S_AVAL:
            k = scan(p, n, quote);
            if (aval) {
                while (k && avlen < avmax) {
                    aval[avlen++] = (char)*p++;
                    --n;
                    --k;
                }
                aval[avlen] = 0;
            }
            p += k;
            n -= k;
            if (n) {
                ++p;
                --n;
                st = S_ATTRS;
            }
            break;

        case S_ENDNAME:
            c = *p++;
            --n;
            if (c == '>') {
                name[nlen] = 0;
                close_tag();
                st = S_TEXT;
            } else if (is_space(c)) {
                name[nlen] = 0;
                st = S_ENDREST;
            } else if (nlen < NAME_MAX) {
                name[nlen++] = (char)c;
            }
            break;

        case S_ENDREST:
            k = scan(p, n, SC_GT);
            p += k;
            n -= k;
            if (n) {
                close_tag();
                st = S_TEXT;
                ++p;
                --n;
            }
            break;

        case S_BANG:
            /* Collect enough to tell [CDATA[ from -- from a declaration. */
            c = *p++;
            --n;
            pi[pilen++] = (char)c;
            if (pilen == 2 && pi[0] == '-' && pi[1] == '-') {
                run = 0;
                st = S_COMMENT;
            } else if (pilen == 7 && !memcmp(pi, "[CDATA[", 7)) {
                run = 0;
                st = S_CDATA;
            } else if ((pilen == 1 && c != '-' && c != '[') ||
                       (pilen >= 2 && pi[0] == '-') ||
                       (pilen >= 2 && memcmp(pi, "[CDATA[", pilen))) {
                st = c == '>' ? S_TEXT : c == '[' ? S_DECLSUB : S_DECL;
            }
            break;

        case S_CDATA:
            /* Copy up to the next ']', then see whether it closes. */
            if (!run) {
                k = scan(p, n, SC_RBRK);
                if (xml_cap) {
                    put_run(p, k);
                }
                p += k;
                n -= k;
                if (!n) {
                    return;
                }
            }
            c = *p++;
            --n;
            if (c == ']') {
                if (run < 2) {
                    ++run;
                } else if (xml_cap) {
                    put_byte(']');      /* "]]]": the first one was text */
                }
            } else if (c == '>' && run == 2) {
                st = S_TEXT;
            } else {
                if (xml_cap) {
                    while (run) {
                        put_byte(']');
                        --run;
                    }
                    put_byte(c);
                }
                run = 0;
            }
            break;

        case S_COMMENT:
            if (!run) {
                k = scan(p, n, SC_DASH);
                p += k;
                n -= k;
                if (!n) {
                    return;
                }
            }
            c = *p++;
            --n;
            if (c == '-') {
                if (run < 2) {
                    ++run;
                }
            } else if (c == '>' && run == 2) {
                st = S_TEXT;
            } else {
                run = 0;
            }
            break;

        case S_DECL:
            k = scan(p, n, SC_GT);
            /* A '[' before the '>' opens an internal subset. */
            {
                const uint8_t *b = memchr(p, '[', k);
                if (b) {
                    k = (uint16_t)(b - p);
                    p += k + 1;
                    n -= k + 1;
                    st = S_DECLSUB;
                    break;
                }
            }
            p += k;
            n -= k;
            if (n) {
                ++p;
                --n;
                st = S_TEXT;
            }
            break;

        case S_DECLSUB:
            c = *p++;
            --n;
            if (c == ']') {
                st = S_DECL;
            }
            break;

        case S_PI:
            c = *p++;
            --n;
            if (c == '>' && pilen && pi[pilen - 1] == '?') {
                --pilen;
                end_pi();
                st = S_TEXT;
            } else if (pilen < PI_MAX) {
                pi[pilen++] = (char)c;
            } else {
                pi[PI_MAX - 1] = (char)c;   /* keep the last byte for '?>' */
            }
            break;
        }
    }
}
