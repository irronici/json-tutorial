#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <stdlib.h>  /* NULL, malloc(), realloc(), free(), strtod() */
#include <string.h>  /* memcpy() */

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)         do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}lept_context;

static void* lept_context_push(lept_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /* c->size * 1.5 */
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++)
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    const char* p = c->json;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else {
        if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

int get_hex4(const char* p){
    const char* c = p;
    unsigned num = 0;
    for(int i=0;i<4;i++){
        fprintf(stderr, "%d: %c\n", i, *c);
        unsigned value = 0;
        if (*c >= '0' && *c <= '9'){
            value = *c - '0';
        }else if(*c >= 'a' && *c <= 'f'){
            value = *c - 'a' + 10;
        }else if(*c >= 'A' && *c <= 'F'){
            value = *c - 'A' + 10;
        }else{
            fprintf(stderr, "in get_hex4, error occur\n");
            return -1;
        }
        num = num * 16 + value;
        c++;
    }
    return num;
}

#define GET_HEX_IN_U(num, u, index)\
do{\
    if((num = get_hex4(c)) != -1){\
        u[index]=num;\
    }else{\
        return NULL;\
    }\
} while (0);

// static const char* lept_parse_hex4(const char* p, unsigned* u) {
//     u = (unsigned*)malloc(2*sizeof(unsigned));
//     unsigned num = 0;
//     const char* c = p;
//     GET_HEX_IN_U(num, u, 0);
//     c+=4;
//     GET_HEX_IN_U(num, u, 1);
//     return c;
// }

static const char* lept_parse_hex4(const char* p, unsigned* u) {
    unsigned num = 0;
    for(int i = 0; i < 4; i++){
        unsigned value = 0;
        fprintf(stderr, "%d: %c\n", i, *p);
        if (*p >= '0' && *p <= '9'){
            value = *p - '0';
        }else if(*p >= 'a' && *p <= 'f'){
            value = *p - 'a' + 10;
        }else if(*p >= 'A' && *p <= 'F'){
            value = *p - 'A' + 10;
        }else{
            fprintf(stderr, "in get_hex4, error occur\n");
            return NULL;
        }
        num = num * 16 + value;
        p++;
    }
    *u = num;
    fprintf(stderr, "in lept_parse_hex4, num: %u, %u\n", num, *u);
    return p;
}

unsigned get_codepoint(unsigned H, unsigned L){
    return 0x10000 + (H - 0xD800) * 0x400 + (L - 0xDC00);
}

static void lept_encode_utf8(lept_context* c, unsigned u) {
    assert(u <= 0x10FFFF && u >= 0x0000);
    if (u<=0x007F){
        PUTC(c,  u       & 0x7F);
    }else if (u >= 0x0080 && u <=0x07FF){
        PUTC(c, 0xC0 | (u >> 6) & 0xDF);
        PUTC(c, 0x80 |  u       & 0xBF);
    }else if (u >= 0x0800 && u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF)); /* 0xE0 = 11100000 */
        PUTC(c, 0x80 | ((u >>  6) & 0x3F)); /* 0x80 = 10000000 */
        PUTC(c, 0x80 | ( u        & 0x3F)); /* 0x3F = 00111111 */
    }else if(u >= 0x10000 && u <= 0x10FFFF){
        PUTC(c, 0xF0 | ((u >> 18) & 0xF7));
        PUTC(c, 0x80 | ((u >> 12) & 0xBF));
        PUTC(c, 0x80 | ((u >> 6 ) & 0xBF));
        PUTC(c, 0x80 | ( u        & 0xBF));
    }
}

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

static int lept_parse_string(lept_context* c, lept_value* v) {
    size_t head = c->top, len;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    for (;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                len = c->top - head;
                lept_set_string(v, (const char*)lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':
                    {
                        unsigned* u = 0;
                        fprintf(stderr, "--------------------\n");
                        if (!(p = lept_parse_hex4(p,&u))){
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        }
                        unsigned value = u;
                        if ((u >= 0xD800) && (u <= 0xDBFF)){   //BMP之外, 用代理对来表示
                            fprintf(stderr, "out of BMP, %u, %u, %u, %u\n", u, (u >= 0xD800 && u <= 0xDBFF), (u >= 0xD800), (u <= 0xDBFF));
                            if (*p++ != '\\'){
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            if (*p++ != 'u'){
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            if (*p == NULL){
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            unsigned low_u = 0;
                            if (!(p = lept_parse_hex4(p,&low_u))){
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            }
                            if (!(u >= 0xD800 && u <= 0xDBFF && low_u >= 0xDC00 && low_u <= 0xDFFF)){
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            value = get_codepoint(u, low_u);
                            fprintf(stderr, "out of BMP, value: %u\n", value);
                        }else{
                            fprintf(stderr, "in BMP, %u, %u, %u, %u\n", u, (u >= 0xD800 && u <= 0xDBFF), (u >= 0xD800), (u <= 0xDBFF));
                        }
                        fprintf(stderr, "value: %u\n", value);
                        lept_encode_utf8(c, value);
                        
                        
                        // fprintf(stderr, "--------------------\n");
                        // unsigned value = get_hex4(p);
                        // if (value == -1){
                        //     STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        // }
                        // fprintf(stderr, "value: %u\n", value);
                        // if (value >= 0xD800){   //BMP之外, 又代理对来表示
                        //     fprintf(stderr, "out of BMP\n");
                        //     const char* tt = p + 4;
                        //     if (tt[0]!='\\' || tt[1]!='u' || tt[2]==NULL){
                        //         STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                        //     }
                        //     tt+=2;
                        //     for (size_t i = 0; i < 4; i++){
                        //         if (*tt == NULL){
                        //             STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                        //         }
                        //         tt++;
                        //     }
                        //     unsigned low_value = get_hex4(tt-4);
                        //     if(low_value == -1){
                        //         STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        //     }else{
                        //         p = tt;
                        //     }
                        //     // if (!(p = lept_parse_hex4(p, &u)))
                        //     //     STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        //     /* \TODO surrogate handling */
                        //     //高代理项: U+D800 至 U+DBFF ; 低代理项: U+DC00 至 U+DFFF
                        //     if (!(value >= 0xD800 && value <= 0xDBFF && low_value >= 0xDC00 && low_value <= 0xDFFF)){
                        //         STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                        //     }
                        //     lept_encode_utf8(c, get_codepoint(value, low_value));
                        // }else{  //BMP之内, 直接作为码点
                        //     fprintf(stderr, "in BMP\n");
                        //     assert(value >= 0 && value < 0xD800);
                        //     lept_encode_utf8(c, value);
                        //     p +=4;
                        // }
                        
                        break;
                    }
                    default:
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20)
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                PUTC(c, ch);
        }
    }
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        default:   return lept_parse_number(c, v);
        case '"':  return lept_parse_string(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

void lept_free(lept_value* v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING)
        free(v->u.s.s);
    v->type = LEPT_NULL;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type == LEPT_TRUE;
}

void lept_set_boolean(lept_value* v, int b) {
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value* v, double n) {
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char*)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}
