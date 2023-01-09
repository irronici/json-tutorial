#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')

typedef struct {
    const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, lept_type type_, char* expected_str) {
    EXPECT(c, expected_str[0]);
    c->json--;
    int len = strlen(expected_str);
    for (int i = 0; i < len; i++){
        if (c->json[i] != expected_str[i]){
            return LEPT_PARSE_INVALID_VALUE;
        }
    }
    c->json += len;
    v->type = type_;
    return LEPT_PARSE_OK;
}

int big_number_check(lept_context* c) {
    int i = 0;
    int len = strlen(c->json);
    while(c->json[i] != 'e' && c->json[i] != 'E'){
        if (++i>=len){
            return LEPT_PARSE_OK;
        }
    }

    char* frac_ptr = (char*)malloc(sizeof(char) * (i+1));
    strncpy(frac_ptr, c->json, i);
    double e = strtod(c->json + i + 1, NULL);
    double frac = strtod(frac_ptr, NULL);

    if (e>308 || (e == 308 && frac > 1.7976931348623157)){
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }else{
        return LEPT_PARSE_OK;
    }
}

int number_check(lept_context* c){
    int big_num_flag = big_number_check(c);
    if (big_num_flag != LEPT_PARSE_OK){
        return big_num_flag;
    }

    if (c->json[0] == '+' || c->json[0] == '.' || (c->json[0] >= 'a' && c->json[0] <= 'z') || (c->json[0] >= 'A' && c->json[0] <= 'Z') ){
        return LEPT_PARSE_INVALID_VALUE;
    }
    if (strlen(c->json) > 1 && c->json[0]=='0' && c->json[1]!='.'){
        return LEPT_PARSE_ROOT_NOT_SINGULAR;
    }
    if (strlen(c->json) == 2 && ISDIGIT1TO9(c->json[0]) && c->json[1]=='.'){
        return LEPT_PARSE_INVALID_VALUE;
    }
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    assert(v != NULL);
    // int flag = number_check(c);
    // if (number_check(c)!=LEPT_PARSE_OK){
    //     return number_check(c);
    // }
    const char *p = c->json;
    if (*p == '-'){
        p++;
    }
    if (*p == '0'){
        p++;
    }else{
        if (!ISDIGIT1TO9(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        while (ISDIGIT1TO9(*p)){
            p++;
        }
    }
    if (*p == '.'){
        p++;
        if (!ISDIGIT(*p)){
            return LEPT_PARSE_INVALID_VALUE;
        }
        while (ISDIGIT(*p)){
            p++;
        }
    }
    if (*p == 'e' || *p == 'E'){
        p++;
        if (*p == '+' || *p == '-'){
            p++;
        }
        if (!ISDIGIT(*p)){
            return LEPT_PARSE_INVALID_VALUE;
        }
        while (ISDIGIT(*p)){
            p++;
        }
    }

    errno = 0;
    v->n = strtod(c->json, NULL);
    if(errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL)){
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    c->json = p;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, LEPT_TRUE, "true");
        case 'f':  return lept_parse_literal(c, v, LEPT_FALSE, "false");
        case 'n':  return lept_parse_literal(c, v, LEPT_NULL, "null");
        default:   return lept_parse_number(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}
