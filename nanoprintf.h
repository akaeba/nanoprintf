/* https://github.com/nothings/stb/blob/master/docs/stb_howto.txt */
#ifndef NANOPRINTF_IMPLEMENTATION

#ifndef NANOPRINTF_H_INCLUDED
#define NANOPRINTF_H_INCLUDED

#ifndef NANOPRINTF_USE_C99_FORMAT_SPECIFIERS
#error NANOPRINTF_USE_C99_FORMAT_SPECIFIERS must be #defined to 0 or 1
#endif

#ifndef NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS
#error NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS must be #defined to 0 or 1
#endif

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Public API */

int npf_snprintf(char *buffer, size_t bufsz, const char *format, ...);
int npf_vsnprintf(char *buffer, size_t bufsz, char const *format,
                  va_list vlist);

enum { NPF_EOF = -1 };
typedef int (*npf_putc)(int c, void *ctx);
int npf_pprintf(npf_putc pc, void *pc_ctx, char const *format, ...);
int npf_vpprintf(npf_putc pc, void *pc_ctx, char const *format, va_list vlist);

/* Internal */

typedef enum {
    NPF_FMT_SPEC_FIELD_WIDTH_NONE,
    NPF_FMT_SPEC_FIELD_WIDTH_LITERAL,
    NPF_FMT_SPEC_FIELD_WIDTH_STAR
} npf__format_spec_field_width_t;

typedef enum {
    NPF_FMT_SPEC_PRECISION_NONE,
    NPF_FMT_SPEC_PRECISION_LITERAL,
    NPF_FMT_SPEC_PRECISION_STAR
} npf__format_spec_precision_t;

typedef enum {
    NPF_FMT_SPEC_LENGTH_MOD_NONE,
    NPF_FMT_SPEC_LENGTH_MOD_SHORT,      /* 'h' */
    NPF_FMT_SPEC_LENGTH_MOD_LONG,       /* 'l' */
    NPF_FMT_SPEC_LENGTH_MOD_LONG_DOUBLE /* 'L' */
#if NANOPRINTF_USE_C99_FORMAT_SPECIFIERS
    ,
    NPF_FMT_SPEC_LENGTH_MOD_C99_CHAR,      /* 'hh' */
    NPF_FMT_SPEC_LENGTH_MOD_C99_LONG_LONG, /* 'll' */
    NPF_FMT_SPEC_LENGTH_MOD_C99_INTMAX,    /* 'j' */
    NPF_FMT_SPEC_LENGTH_MOD_C99_SIZET,     /* 'z' */
    NPF_FMT_SPEC_LENGTH_MOD_C99_PTRDIFFT   /* 't' */
#endif
} npf__format_spec_length_modifier_t;

typedef enum {
    NPF_FMT_SPEC_CONV_PERCENT,       /* '%' */
    NPF_FMT_SPEC_CONV_CHAR,          /* 'c' */
    NPF_FMT_SPEC_CONV_STRING,        /* 's' */
    NPF_FMT_SPEC_CONV_SIGNED_INT,    /* 'i', 'd' */
    NPF_FMT_SPEC_CONV_OCTAL,         /* 'o' */
    NPF_FMT_SPEC_CONV_HEX_INT,       /* 'x', 'X' */
    NPF_FMT_SPEC_CONV_UNSIGNED_INT,  /* 'u' */
    NPF_FMT_SPEC_CONV_CHARS_WRITTEN, /* 'n' */
    NPF_FMT_SPEC_CONV_POINTER        /* 'p' */
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS
    ,
    NPF_FMT_SPEC_CONV_FLOAT_DECIMAL,  /* 'f', 'F' */
    NPF_FMT_SPEC_CONV_FLOAT_EXPONENT, /* 'e', 'E' */
    NPF_FMT_SPEC_CONV_FLOAT_DYNAMIC   /* 'g', 'G' */
#if NANOPRINTF_USE_C99_FORMAT_SPECIFIERS
    ,
    NPF_FMT_SPEC_CONV_C99_FLOAT_HEX /* 'a', 'A' */
#endif
#endif
} npf__format_spec_conversion_t;

typedef enum {
    NPF_FMT_SPEC_CONV_CASE_NONE,
    NPF_FMT_SPEC_CONV_CASE_LOWER,
    NPF_FMT_SPEC_CONV_CASE_UPPER
} npf__format_spec_conversion_case_t;

typedef struct {
    /* optional flags */
    unsigned char left_justified : 1;   /* '-' */
    unsigned char prepend_sign : 1;     /* '+' */
    unsigned char prepend_space : 1;    /* ' ' */
    unsigned char alternative_form : 1; /* '#' */
    unsigned char leading_zero_pad : 1; /* '0' */

    /* field width */
    npf__format_spec_field_width_t field_width_type;
    int field_width;

    /* precision */
    npf__format_spec_precision_t precision_type;
    int precision;

    /* length modifier for specifying argument size */
    npf__format_spec_length_modifier_t length_modifier;

    /* conversion specifiers */
    npf__format_spec_conversion_t conversion_specifier;
    npf__format_spec_conversion_case_t conversion_specifier_case;
} npf__format_spec_t;

int npf__parse_format_spec(char const *format, npf__format_spec_t *out_spec);

typedef struct {
    char *dst;
    size_t len;
    size_t cur;
} npf__bufputc_ctx_t;

int npf__bufputc(int c, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* NANOPRINTF_H_INCLUDED */

#else /* NANOPRINTF_IMPLEMENTATION */

#undef NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"
#define NANOPRINTF_IMPLEMENTATION

#if NANOPRINTF_USE_C99_FORMAT_SPECIFIERS
#include <inttypes.h>
#include <stdint.h>
#include <wchar.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

int npf__parse_format_spec(char const *format, npf__format_spec_t *out_spec) {
    char const *cur = format;

    out_spec->left_justified = 0;
    out_spec->prepend_sign = 0;
    out_spec->prepend_space = 0;
    out_spec->alternative_form = 0;
    out_spec->leading_zero_pad = 0;
    out_spec->field_width_type = NPF_FMT_SPEC_FIELD_WIDTH_NONE;
    out_spec->precision_type = NPF_FMT_SPEC_PRECISION_NONE;
    out_spec->length_modifier = NPF_FMT_SPEC_LENGTH_MOD_NONE;

    /* Format specifiers start with % */
    if (*cur++ != '%') {
        return 0;
    }

    /* Optional flags */
    while (*cur == '-' || *cur == '+' || *cur == ' ' || *cur == '#' ||
           *cur == '0') {
        switch (*cur++) {
            case '-':
                out_spec->left_justified = 1;
                out_spec->leading_zero_pad = 0;
                break;
            case '+':
                out_spec->prepend_sign = 1;
                out_spec->prepend_space = 0;
                break;
            case ' ':
                out_spec->prepend_space = !out_spec->prepend_sign;
                break;
            case '#':
                out_spec->alternative_form = 1;
                break;
            case '0':
                out_spec->leading_zero_pad = !out_spec->left_justified;
                break;
        }
    }

    /* Minimum field width */
    if (*cur == '*') {
        out_spec->field_width_type = NPF_FMT_SPEC_FIELD_WIDTH_STAR;
        ++cur;
    } else {
        if (*cur >= '0' && *cur <= '9') {
            out_spec->field_width_type = NPF_FMT_SPEC_FIELD_WIDTH_LITERAL;
        }
        while (*cur >= '0' && *cur <= '9') {
            out_spec->field_width =
                (out_spec->field_width * 10) + (*cur++ - '0');
        }
    }

    /* Precision */
    out_spec->precision = 0;
    if (*cur == '.') {
        ++cur;
        out_spec->precision_type = NPF_FMT_SPEC_PRECISION_LITERAL;
        if (*cur == '*') {
            ++cur;
            out_spec->precision_type = NPF_FMT_SPEC_PRECISION_STAR;
        } else {
            while (*cur >= '0' && *cur <= '9') {
                out_spec->precision =
                    (out_spec->precision * 10) + (*cur++ - '0');
            }
        }
    }

    /* Length modifier */
    if (*cur == 'h' || *cur == 'l'
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS
        || *cur == 'L'
#endif
#if NANOPRINTF_USE_C99_FORMAT_SPECIFIERS
        || *cur == 'j' || *cur == 'z' || *cur == 't'
#endif
    ) {
        switch (*cur++) {
            case 'h':
#if NANOPRINTF_USE_C99_FORMAT_SPECIFIERS
                if (*cur == 'h') {
                    out_spec->length_modifier =
                        NPF_FMT_SPEC_LENGTH_MOD_C99_CHAR;
                    ++cur;
                } else
#endif
                    out_spec->length_modifier = NPF_FMT_SPEC_LENGTH_MOD_SHORT;
                break;
            case 'l':
#if NANOPRINTF_USE_C99_FORMAT_SPECIFIERS
                if (*cur == 'l') {
                    out_spec->length_modifier =
                        NPF_FMT_SPEC_LENGTH_MOD_C99_LONG_LONG;
                    ++cur;
                } else
#endif
                    out_spec->length_modifier = NPF_FMT_SPEC_LENGTH_MOD_LONG;
                break;
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS
            case 'L':
                out_spec->length_modifier = NPF_FMT_SPEC_LENGTH_MOD_LONG_DOUBLE;
                break;
#endif
#if NANOPRINTF_USE_C99_FORMAT_SPECIFIERS
            case 'j':
                out_spec->length_modifier = NPF_FMT_SPEC_LENGTH_MOD_C99_INTMAX;
                break;
            case 'z':
                out_spec->length_modifier = NPF_FMT_SPEC_LENGTH_MOD_C99_SIZET;
                break;
            case 't':
                out_spec->length_modifier =
                    NPF_FMT_SPEC_LENGTH_MOD_C99_PTRDIFFT;
                break;
#endif
        }
    }

    /* Conversion specifier */
    switch (*cur++) {
        case '%':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_PERCENT;
            break;
        case 'c':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_CHAR;
            break;
        case 's':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_STRING;
            break;
        case 'i':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_SIGNED_INT;
            break;
        case 'd':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_SIGNED_INT;
            break;
        case 'o':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_OCTAL;
            break;
        case 'x':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_HEX_INT;
            out_spec->conversion_specifier_case = NPF_FMT_SPEC_CONV_CASE_LOWER;
            break;
        case 'X':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_HEX_INT;
            out_spec->conversion_specifier_case = NPF_FMT_SPEC_CONV_CASE_UPPER;
            break;
        case 'u':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_UNSIGNED_INT;
            break;
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS
        case 'f':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_FLOAT_DECIMAL;
            out_spec->conversion_specifier_case = NPF_FMT_SPEC_CONV_CASE_LOWER;
            break;
        case 'F':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_FLOAT_DECIMAL;
            out_spec->conversion_specifier_case = NPF_FMT_SPEC_CONV_CASE_UPPER;
            break;
        case 'e':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_FLOAT_EXPONENT;
            out_spec->conversion_specifier_case = NPF_FMT_SPEC_CONV_CASE_LOWER;
            break;
        case 'E':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_FLOAT_EXPONENT;
            out_spec->conversion_specifier_case = NPF_FMT_SPEC_CONV_CASE_UPPER;
            break;
#if NANOPRINTF_USE_C99_FORMAT_SPECIFIERS
        case 'a':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_C99_FLOAT_HEX;
            out_spec->conversion_specifier_case = NPF_FMT_SPEC_CONV_CASE_LOWER;
            break;
        case 'A':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_C99_FLOAT_HEX;
            out_spec->conversion_specifier_case = NPF_FMT_SPEC_CONV_CASE_UPPER;
            break;
#endif
        case 'g':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_FLOAT_DYNAMIC;
            out_spec->conversion_specifier_case = NPF_FMT_SPEC_CONV_CASE_LOWER;
            break;
        case 'G':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_FLOAT_DYNAMIC;
            out_spec->conversion_specifier_case = NPF_FMT_SPEC_CONV_CASE_UPPER;
            break;
#endif
        case 'n':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_CHARS_WRITTEN;
            break;
        case 'p':
            out_spec->conversion_specifier = NPF_FMT_SPEC_CONV_POINTER;
            break;
        default:
            return 0;
    }
    return (int)(cur - format);
}

int npf__bufputc(int c, void *ctx) {
    npf__bufputc_ctx_t *bpc = (npf__bufputc_ctx_t *)ctx;
    if (bpc->cur < bpc->len) {
        bpc->dst[bpc->cur++] = (char)c;
        return c;
    }
    return NPF_EOF;
}

int npf_vpprintf(npf_putc pc, void *pc_ctx, char const *format, va_list vlist) {
    int n = 0;
    char const *cur = format;
    while (*cur) {
        if (*cur != '%') {
            if (pc(*cur++, pc_ctx) == NPF_EOF) {
                return n;
            }
            ++n;
        } else {
            npf__format_spec_t fs;
            int const fs_len = npf__parse_format_spec(cur, &fs);
            if (fs_len == 0) {
                if (pc(*cur++, pc_ctx) == NPF_EOF) {
                    return n;
                }
                ++n;
            } else {
                switch (fs.conversion_specifier) {
                    case NPF_FMT_SPEC_CONV_PERCENT:
                        if (pc('%', pc_ctx) == NPF_EOF) {
                            return n;
                        }
                        ++n;
                        break;
                    case NPF_FMT_SPEC_CONV_CHAR: /* 'c' */
                        if (pc((char)va_arg(vlist, int), pc_ctx) == NPF_EOF) {
                            return n;
                        }
                        ++n;
                        break;
                    case NPF_FMT_SPEC_CONV_STRING: { /* 's' */
                        char const *s = va_arg(vlist, char *);
                        while (*s) {
                            if (pc(*s++, pc_ctx) == NPF_EOF) {
                                return n;
                            }
                            ++n;
                        }
                    } break;
                    case NPF_FMT_SPEC_CONV_SIGNED_INT: { /* 'i', 'd' */
                        int i = va_arg(vlist, int);
                        char ibuf[24], *dst = ibuf;
                        if (i == 0) {
                            *dst++ = '0';
                        } else {
                            if (i < 0) {
                                if (pc('-', pc_ctx) == NPF_EOF) {
                                    return n;
                                }
                                ++n;
                                i = -i;
                            }
                            while (i) {
                                *dst++ = '0' + (i % 10);
                                i /= 10;
                            }
                        }
                        while (dst > ibuf) {
                            if (pc(*--dst, pc_ctx) == NPF_EOF) {
                                return n;
                            }
                            ++n;
                        }
                    } break;
                    case NPF_FMT_SPEC_CONV_OCTAL: /* 'o' */
                        break;
                    case NPF_FMT_SPEC_CONV_HEX_INT: /* 'x', 'X' */
                        break;
                    case NPF_FMT_SPEC_CONV_UNSIGNED_INT: /* 'u' */
                        break;
                    case NPF_FMT_SPEC_CONV_CHARS_WRITTEN: /* 'n' */
                        break;
                    case NPF_FMT_SPEC_CONV_POINTER: /* 'p' */
                        break;
#if NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS
                    case NPF_FMT_SPEC_CONV_FLOAT_DECIMAL: /* 'f', 'F' */
                        break;
                    case NPF_FMT_SPEC_CONV_FLOAT_EXPONENT: /* 'e', 'E' */
                        break;
                    case NPF_FMT_SPEC_CONV_FLOAT_DYNAMIC: /* 'g', 'G' */
                        break;
#if NANOPRINTF_USE_C99_FORMAT_SPECIFIERS
                    case NPF_FMT_SPEC_CONV_C99_FLOAT_HEX: /* 'a', 'A' */
                        break;
#endif
#endif
                }
                cur += fs_len;
            }
        }
    }
    if (pc('\0', pc_ctx) == NPF_EOF) {
        return n;
    }
    return n + 1;
}

int npf_pprintf(npf_putc pc, void *pc_ctx, char const *format, ...) {
    va_list val;
    int rv;
    va_start(val, format);
    rv = npf_vpprintf(pc, pc_ctx, format, val);
    va_end(val);
    return rv;
}

int npf_snprintf(char *buffer, size_t bufsz, const char *format, ...) {
    va_list val;
    int rv;
    va_start(val, format);
    rv = npf_vsnprintf(buffer, bufsz, format, val);
    va_end(val);
    return rv;
}

int npf_vsnprintf(char *buffer, size_t bufsz, char const *format,
                  va_list vlist) {
    npf__bufputc_ctx_t bufputc_ctx;
    bufputc_ctx.dst = buffer;
    bufputc_ctx.len = bufsz;
    bufputc_ctx.cur = 0;
    return npf_vpprintf(npf__bufputc, &bufputc_ctx, format, vlist);
}

#ifdef __cplusplus
}
#endif

#endif /* NANOPRINTF_IMPLEMENTATION */
