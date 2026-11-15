/*
 * Feed text to screen text. See clean.h.
 */
#include <string.h>
#include "clean.h"

/* ---- folding Unicode to ASCII ---- */

/* U+00C0..U+00FF, two characters each ("A " for one letter). Each row is
 * exactly 32 characters; the host tests check the total. */
static const char latin1[] =
    "A A A A A A AEC E E E E I I I I "     /* C0-CF */
    "D N O O O O O x O U U U U Y Thss"     /* D0-DF */
    "a a a a a a aec e e e e i i i i "     /* E0-EF */
    "d n o o o o o / o u u u u y thy ";    /* F0-FF */

const uint16_t clean_latin1_len = sizeof latin1 - 1;

/* U+0100..U+017F, from the Unicode decompositions. */
static const char latin_ext_a[] =
    "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkkLlLlLlLlLl"
    "NnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";

struct fold { uint16_t cp; char s[4]; };

/* Everything else with a fair ASCII equivalent, sorted by code point. */
static const struct fold folds[] = {
    { 0x00A0, " "   }, { 0x00A1, "!"   }, { 0x00A2, "c"   }, { 0x00A3, "L"   },
    { 0x00A5, "Y"   }, { 0x00A7, "S"   }, { 0x00A9, "(c)" }, { 0x00AB, "\""  },
    { 0x00AD, ""    }, { 0x00AE, "(R)" }, { 0x00B0, "o"   }, { 0x00B1, "+-"  },
    { 0x00B2, "2"   }, { 0x00B3, "3"   }, { 0x00B4, "'"   }, { 0x00B5, "u"   },
    { 0x00B6, "P"   }, { 0x00B7, "."   }, { 0x00B9, "1"   }, { 0x00BB, "\""  },
    { 0x00BC, "1/4" }, { 0x00BD, "1/2" }, { 0x00BE, "3/4" }, { 0x00BF, "?"   },
    { 0x0152, "OE"  }, { 0x0153, "oe"  },
    { 0x2002, " "   }, { 0x2003, " "   }, { 0x2004, " "   }, { 0x2005, " "   },
    { 0x2006, " "   }, { 0x2007, " "   }, { 0x2008, " "   }, { 0x2009, " "   },
    { 0x200A, " "   }, { 0x200B, ""    }, { 0x200C, ""    }, { 0x200D, ""    },
    { 0x2010, "-"   }, { 0x2011, "-"   }, { 0x2012, "-"   }, { 0x2013, "-"   },
    { 0x2014, "--"  }, { 0x2015, "--"  }, { 0x2018, "'"   }, { 0x2019, "'"   },
    { 0x201A, "'"   }, { 0x201B, "'"   }, { 0x201C, "\""  }, { 0x201D, "\""  },
    { 0x201E, "\""  }, { 0x201F, "\""  }, { 0x2020, "+"   }, { 0x2022, "*"   },
    { 0x2026, "..." }, { 0x202F, " "   }, { 0x2032, "'"   }, { 0x2033, "\""  },
    { 0x2039, "<"   }, { 0x203A, ">"   }, { 0x2060, ""    }, { 0x20AC, "EUR" },
    { 0x2122, "TM"  }, { 0x2190, "<-"  }, { 0x2192, "->"  }, { 0x2212, "-"   },
    { 0x2248, "~"   }, { 0x2260, "!="  }, { 0x2264, "<="  }, { 0x2265, ">="  },
    { 0xFEFF, ""    },
};
#define NFOLDS  (sizeof folds / sizeof folds[0])

static char one[3];

const char *clean_fold(uint16_t cp)
{
    uint8_t lo, hi, mid;

    if (cp < 0x80) {
        one[0] = (char)cp;
        one[1] = 0;
        return one;
    }
    if (cp >= 0xC0 && cp <= 0xFF && cp != 0xD7 && cp != 0xF7) {
        one[0] = latin1[(cp - 0xC0) * 2];
        one[1] = latin1[(cp - 0xC0) * 2 + 1];
        if (one[1] == ' ') {
            one[1] = 0;
        }
        one[2] = 0;
        return one;
    }
    if (cp == 0xD7) return "x";
    if (cp == 0xF7) return "/";
    if (cp >= 0x100 && cp < 0x180 && cp != 0x152 && cp != 0x153) {
        one[0] = latin_ext_a[cp - 0x100];
        one[1] = 0;
        return one;
    }
    if (cp >= 0x300 && cp < 0x370) {
        return "";                  /* combining accents: the letter came first */
    }
    lo = 0;
    hi = NFOLDS;
    while (lo < hi) {
        mid = (uint8_t)((lo + hi) / 2);
        if (folds[mid].cp == cp) {
            return folds[mid].s;
        }
        if (folds[mid].cp < cp) {
            lo = (uint8_t)(mid + 1);
        } else {
            hi = mid;
        }
    }
    return "?";
}

/* ---- HTML entities ---- */

struct ent { char name[7]; uint16_t cp; };

static const struct ent ents[] = {
    { "nbsp", 0xA0 },   { "amp", '&' },     { "lt", '<' },      { "gt", '>' },
    { "quot", '"' },    { "apos", '\'' },   { "rsquo", 0x2019 },{ "lsquo", 0x2018 },
    { "rdquo", 0x201D },{ "ldquo", 0x201C },{ "sbquo", 0x201A },{ "bdquo", 0x201E },
    { "mdash", 0x2014 },{ "ndash", 0x2013 },{ "hellip", 0x2026 },{ "bull", 0x2022 },
    { "middot", 0xB7 }, { "copy", 0xA9 },   { "reg", 0xAE },    { "trade", 0x2122 },
    { "times", 0xD7 },  { "divide", 0xF7 }, { "deg", 0xB0 },    { "laquo", 0xAB },
    { "raquo", 0xBB },  { "lsaquo", 0x2039 },{ "rsaquo", 0x203A },{ "euro", 0x20AC },
    { "pound", 0xA3 },  { "yen", 0xA5 },    { "cent", 0xA2 },   { "sect", 0xA7 },
    { "para", 0xB6 },   { "frac12", 0xBD }, { "frac14", 0xBC }, { "frac34", 0xBE },
    { "eacute", 0xE9 }, { "egrave", 0xE8 }, { "aacute", 0xE1 }, { "agrave", 0xE0 },
    { "iacute", 0xED }, { "oacute", 0xF3 }, { "uacute", 0xFA }, { "ouml", 0xF6 },
    { "uuml", 0xFC },   { "auml", 0xE4 },   { "ccedil", 0xE7 }, { "ntilde", 0xF1 },
    { "szlig", 0xDF },  { "shy", 0xAD },    { "thinsp", 0x2009 },{ "ensp", 0x2002 },
    { "emsp", 0x2003 }, { "zwj", 0x200D },  { "zwnj", 0x200C }, { "prime", 0x2032 },
    { "minus", 0x2212 },{ "hairsp", 0x200A },
};
#define NENTS   (sizeof ents / sizeof ents[0])

/* The code point of "&...;" at s (n bytes available), and its length in
 * *len. 0 if it is not one. */
static uint16_t html_entity(const char *s, uint16_t n, uint8_t *len)
{
    char name[9];
    uint8_t i, k, big = 0;
    uint16_t v = 0;
    char c;

    for (k = 1; k < n && k < 10 && s[k] != ';'; ++k) {
    }
    if (k >= n || s[k] != ';' || k < 2) {
        return 0;
    }
    *len = (uint8_t)(k + 1);
    if (s[1] == '#') {
        if (s[2] == 'x' || s[2] == 'X') {
            for (i = 3; i < k; ++i) {
                c = (char)(s[i] | 0x20);
                if (v > 0x0FFF) big = 1;
                v <<= 4;
                if (s[i] >= '0' && s[i] <= '9') v |= (uint8_t)(s[i] - '0');
                else if (c >= 'a' && c <= 'f') v |= (uint8_t)(c - 'a' + 10);
                else return 0;
            }
        } else {
            for (i = 2; i < k; ++i) {
                if (s[i] < '0' || s[i] > '9') return 0;
                if (v > 6553) big = 1;
                v = (uint16_t)(v * 10 + (uint8_t)(s[i] - '0'));
            }
        }
        return big ? 0xFFFD : v;
    }
    if (k - 1 > 8) {
        return 0;
    }
    memcpy(name, s + 1, k - 1);
    name[k - 1] = 0;
    for (i = 0; i < NENTS; ++i) {
        if (!strcmp(ents[i].name, name)) {
            return ents[i].cp;
        }
    }
    return 0;
}

/* ---- HTML tags ---- */

#define T_INLINE    0   /* <a>, <span>, <b>: nothing */
#define T_BREAK     1   /* <p>, <br>, <div>, ...: a line break */
#define T_ITEM      2   /* <li>: a break and "- " */
#define T_SPACE     3   /* <td>, <th>: a space */
#define T_SKIP      4   /* <script>, <style>: drop everything to the close */

static const char *const breaks[] = {
    "p", "br", "div", "tr", "ul", "ol", "h1", "h2", "h3", "h4", "h5", "h6",
    "blockquote", "figure", "figcaption", "table", "section", "article",
    "header", "footer", "pre", "hr", "dd", "dt", "dl"
};
#define NBREAKS (sizeof breaks / sizeof breaks[0])

static uint8_t tag_kind(const char *name, uint8_t closing)
{
    uint8_t i;

    if (!strcmp(name, "li")) {
        return closing ? T_BREAK : T_ITEM;
    }
    if (!strcmp(name, "td") || !strcmp(name, "th")) {
        return T_SPACE;
    }
    if (!closing && (!strcmp(name, "script") || !strcmp(name, "style"))) {
        return T_SKIP;
    }
    for (i = 0; i < NBREAKS; ++i) {
        if (!strcmp(breaks[i], name)) {
            return T_BREAK;
        }
    }
    return T_INLINE;
}

/* Find "</name" (any case) at or after s. Returns its offset, or n. */
static uint16_t find_close(const char *s, uint16_t n, const char *name)
{
    uint8_t len = (uint8_t)strlen(name), j;
    uint16_t i;

    for (i = 0; i + 2 + len <= n; ++i) {
        if (s[i] == '<' && s[i + 1] == '/') {
            for (j = 0; j < len && (s[i + 2 + j] | 0x20) == name[j]; ++j) {
            }
            if (j == len) {
                return i;
            }
        }
    }
    return n;
}

/* ---- the cleaner ---- */

static char    *out;
static uint16_t olen, ocap;
static uint8_t  pend;           /* 0 nothing, 1 space, 2 line break */
static uint8_t  cmode;

static void emit_sep(void)
{
    if (pend && olen) {
        if (olen + 1 < ocap) {
            out[olen++] = pend == 2 && cmode == CLEAN_BODY ? '\n' : ' ';
        }
    }
    pend = 0;
}

static void emit(const char *s)
{
    if (!*s) {
        return;
    }
    emit_sep();
    while (*s && olen + 1 < ocap) {
        out[olen++] = *s++;
    }
}

static void emit_cp(uint16_t cp)
{
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' ||
        cp == 0xA0 || (cp >= 0x2002 && cp <= 0x200A) || cp == 0x202F) {
        if (!pend) {
            pend = 1;
        }
        return;
    }
    if (cp < 0x20 || cp == 0x7F) {
        return;
    }
    emit(clean_fold(cp));
}

static void brk(void)
{
    pend = 2;
}

uint16_t clean(char *dst, uint16_t cap, const char *src, uint16_t n, uint8_t mode)
{
    uint16_t i = 0, k;
    uint8_t c, need, len, kind, closing, nl;
    uint16_t cp;
    char tname[12];
    const char *gt;

    out = dst;
    olen = 0;
    ocap = cap;
    pend = 0;
    cmode = mode;

    while (i < n && olen + 1 < ocap) {
        c = (uint8_t)src[i];

        if (c == '<') {
            /* A tag if a name, '/' or '!' follows; otherwise a literal
             * '<' ("1 < 2"). A tag with no '>' after it was cut off by the
             * capture cap, so the text ends there. */
            c = i + 1 < n ? (uint8_t)src[i + 1] : 0;
            if (!(((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || c == '/' || c == '!')) {
                emit("<");
                ++i;
                continue;
            }
            gt = memchr(src + i, '>', n - i);
            if (!gt) {
                break;
            }
            if (src[i + 1] == '!' && i + 3 < n && src[i + 2] == '-' && src[i + 3] == '-') {
                /* <!-- comment -->: to the "-->", or the end */
                for (k = (uint16_t)(i + 4); k + 2 < n; ++k) {
                    if (src[k] == '-' && src[k + 1] == '-' && src[k + 2] == '>') {
                        break;
                    }
                }
                i = k + 3 > n ? n : (uint16_t)(k + 3);
                continue;
            }
            closing = (uint8_t)(src[i + 1] == '/');
            k = (uint16_t)(i + 1 + closing);
            for (nl = 0; k < n && nl < sizeof tname - 1; ++k, ++nl) {
                c = (uint8_t)src[k];
                if (!((c | 0x20) >= 'a' && (c | 0x20) <= 'z') && !(c >= '0' && c <= '9')) {
                    break;
                }
                tname[nl] = (char)(c | 0x20);
            }
            tname[nl] = 0;
            i = (uint16_t)(gt - src + 1);
            kind = tag_kind(tname, closing);
            if (kind == T_SKIP) {
                k = find_close(src + i, (uint16_t)(n - i), tname);
                i += k;
                gt = memchr(src + i, '>', n - i);
                i = gt ? (uint16_t)(gt - src + 1) : n;
            } else if (kind == T_BREAK) {
                brk();
            } else if (kind == T_ITEM) {
                brk();
                if (mode == CLEAN_BODY) {
                    emit("-");
                    pend = 1;
                }
            } else if (kind == T_SPACE) {
                if (!pend) {
                    pend = 1;
                }
            }
            continue;
        }

        if (c == '&') {
            cp = html_entity(src + i, (uint16_t)(n - i), &len);
            if (cp) {
                emit_cp(cp);
                i += len;
            } else {
                emit("&");
                ++i;
            }
            continue;
        }

        if (c < 0x80) {
            emit_cp(c);
            ++i;
            continue;
        }

        /* UTF-8 */
        if ((c & 0xE0) == 0xC0) {
            need = 1;
            cp = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            need = 2;
            cp = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            need = 3;               /* past U+FFFF: becomes U+FFFD, '?' */
            cp = 0;
        } else {
            emit("?");              /* a stray continuation byte */
            ++i;
            continue;
        }
        for (k = 1; k <= need; ++k) {
            if (i + k >= n || ((uint8_t)src[i + k] & 0xC0) != 0x80) {
                break;
            }
            cp = (uint16_t)((cp << 6) | ((uint8_t)src[i + k] & 0x3F));
        }
        if (k <= need) {
            emit("?");              /* cut short */
            i += k;
            continue;
        }
        emit_cp(need == 3 ? 0xFFFD : cp);
        i += (uint16_t)(need + 1);
    }

    dst[olen] = 0;
    return olen;
}

void clean_trim(char *s)
{
    char *b = s, *e;

    while (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n') {
        ++b;
    }
    e = b + strlen(b);
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) {
        --e;
    }
    *e = 0;
    if (b != s) {
        memmove(s, b, (size_t)(e - b + 1));
    }
}

/* ---- dates ---- */

static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

/* Read up to max digits; returns the count read. */
static uint8_t digits(const char **ps, uint16_t *v, uint8_t max)
{
    const char *s = *ps;
    uint8_t n = 0;

    *v = 0;
    while (n < max && *s >= '0' && *s <= '9') {
        *v = (uint16_t)(*v * 10 + (uint8_t)(*s++ - '0'));
        ++n;
    }
    *ps = s;
    return n;
}

static char *put2(char *p, uint16_t v)
{
    *p++ = (char)('0' + v / 10 % 10);
    *p++ = (char)('0' + v % 10);
    return p;
}

uint8_t clean_date(char *dst, const char *s)
{
    uint16_t y, mo = 0, d, h = 0, mi = 0;
    uint8_t i, n;
    char *p;
    const char *start;

    dst[0] = 0;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
        ++s;
    }
    start = s;
    if (digits(&s, &y, 4) == 4 && *s == '-') {
        /* ISO 8601 */
        ++s;
        if (digits(&s, &mo, 2) != 2 || *s++ != '-' || digits(&s, &d, 2) != 2) {
            return 0;
        }
        if ((*s == 'T' || *s == ' ') && (++s, digits(&s, &h, 2) == 2) && *s == ':') {
            ++s;
            digits(&s, &mi, 2);
        }
    } else {
        /* RFC 822: [Day,] DD Mon YY[YY] HH:MM[:SS] zone. The ISO attempt
         * may have read the day's digits, so start again. */
        s = start;
        while (*s && !(*s >= '0' && *s <= '9')) {
            if (*s == ',') {
                ++s;
                break;
            }
            ++s;
        }
        while (*s == ' ') {
            ++s;
        }
        if (!digits(&s, &d, 2)) {
            return 0;
        }
        while (*s == ' ' || *s == '-') {
            ++s;
        }
        for (i = 0; i < 12; ++i) {
            if ((s[0] | 0x20) == (months[i * 3] | 0x20) &&
                (s[1] | 0x20) == months[i * 3 + 1] &&
                (s[2] | 0x20) == months[i * 3 + 2]) {
                mo = (uint16_t)(i + 1);
                break;
            }
        }
        if (!mo) {
            return 0;
        }
        while (*s && *s != ' ' && *s != '-') {
            ++s;
        }
        while (*s == ' ' || *s == '-') {
            ++s;
        }
        n = digits(&s, &y, 4);
        if (n == 2) {
            y = (uint16_t)(y < 70 ? 2000 + y : 1900 + y);
        } else if (n != 4) {
            return 0;
        }
        while (*s == ' ') {
            ++s;
        }
        if (digits(&s, &h, 2) && *s == ':') {
            ++s;
            digits(&s, &mi, 2);
        } else {
            h = 0;
        }
    }
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59) {
        return 0;
    }
    p = dst;
    *p++ = (char)('0' + y / 1000 % 10);
    *p++ = (char)('0' + y / 100 % 10);
    p = put2(p, y);
    *p++ = '-';
    p = put2(p, mo);
    *p++ = '-';
    p = put2(p, d);
    *p++ = ' ';
    p = put2(p, h);
    *p++ = ':';
    p = put2(p, mi);
    *p = 0;
    return 1;
}
