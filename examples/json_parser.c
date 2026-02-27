#include "json_parser.h"
#include "String.h"
#include "gen_vector.h"
#include "hashmap.h"
#include "map_setup.h"
#include "wc_helpers.h"
#include "wc_macros.h"
#include <ctype.h>
#include <limits.h>
#include <math.h>



/*
 * WCtoolkit callbacks — must be registered in every container that holds
 * JsonValues. With them in place, push/pop/copy/destroy of the container
 * automatically handles the full recursive tree.
*/
void json_val_copy(u8* dest, const u8* src);
void json_val_move(u8* dest, u8** src);
void json_val_del(u8* elm);
void json_val_print(const u8* elm);


// construct ops struct for json (by value)
container_ops json_val_ops = {
    .copy_fn = json_val_copy,
    .move_fn = json_val_move,
    .del_fn  = json_val_del
};

// construct ops struct for string (by value)
container_ops str_val_ops = {
    .copy_fn = str_copy,
    .move_fn = str_move,
    .del_fn  = str_del
};

// WCtoolkit callbacks for JsonValue

void json_val_copy(u8* dest, const u8* src)
{
    const JsonValue* s = (const JsonValue*)src;
    JsonValue*       d = (JsonValue*)dest;
    d->type            = s->type;
    switch (s->type) {
    case JSON_NULL:
        break;
    case JSON_BOOL:
        d->boolean = s->boolean;
        break;
    case JSON_NUMBER:
        d->number = s->number;
        break;
    case JSON_STRING:
        string_copy(&d->string, &s->string);
        break;
    case JSON_ARRAY: {
        /*
         * Allocate a fresh empty vec with the right ops, then copy into it.
         * Do NOT pre-size with s->array->capacity — genVec_copy handles
         * sizing internally and would destroy the pre-allocated (uninitialised)
         * data if we did.
         */
        d->array = genVec_init(0, sizeof(JsonValue), &json_val_ops);
        genVec_copy(d->array, s->array);
    } break;
    case JSON_OBJECT: {
        d->object = hashmap_create(sizeof(String), sizeof(JsonValue),
                                   murmurhash3_str, str_cmp,
                                   &str_val_ops, &json_val_ops);
        hashmap_copy(d->object, s->object);
    } break;
    }
}

void json_val_move(u8* dest, u8** src)
{
    memcpy(dest, *src, sizeof(JsonValue));
    free(*src);
    *src = NULL;
}

void json_val_del(u8* elm)
{
    JsonValue* val = (JsonValue*)elm;
    switch (val->type) {
    case JSON_NULL:
    case JSON_BOOL:
    case JSON_NUMBER:
        break;
    case JSON_STRING:
        string_destroy_stk(&val->string);
        break;
    case JSON_ARRAY:
        genVec_destroy(val->array);
        val->array = NULL;
        break;
    case JSON_OBJECT:
        hashmap_destroy(val->object);
        val->object = NULL;
        break;
    }
}

void json_val_print(const u8* elm)
{
    json_print((const JsonValue*)elm, 2);
    putchar('\n');
}


// Lifecycle

void json_value_destroy(JsonValue* val)
{
    if (!val) {
        return;
    }
    json_val_del((u8*)val);
    free(val);
}

void json_value_destroy_stk(JsonValue* val)
{
    if (!val) {
        return;
    }
    json_val_del((u8*)val);
}

JsonValue* json_value_copy(const JsonValue* src)
{
    if (!src) {
        return NULL;
    }
    JsonValue* d = malloc(sizeof(JsonValue));
    json_val_copy((u8*)d, (const u8*)src);
    return d;
}

// Constructors

JsonValue* json_null(void)
{
    JsonValue* v = malloc(sizeof(JsonValue));
    v->type      = JSON_NULL;
    return v;
}

JsonValue* json_bool(b8 val)
{
    JsonValue* v = malloc(sizeof(JsonValue));
    v->type      = JSON_BOOL;
    v->boolean   = val;
    return v;
}

JsonValue* json_number(double val)
{
    JsonValue* v = malloc(sizeof(JsonValue));
    v->type      = JSON_NUMBER;
    v->number    = val;
    return v;
}

JsonValue* json_string_val(const char* cstr)
{
    JsonValue* v = malloc(sizeof(JsonValue));
    v->type      = JSON_STRING;
    string_create_stk(&v->string, cstr);
    return v;
}

JsonValue* json_array_new(void)
{
    JsonValue* v = malloc(sizeof(JsonValue));
    v->type      = JSON_ARRAY;
    v->array     = genVec_init(8, sizeof(JsonValue), &json_val_ops);
    return v;
}

JsonValue* json_object_new(void)
{
    JsonValue* v = malloc(sizeof(JsonValue));
    v->type      = JSON_OBJECT;
    v->object    = hashmap_create(sizeof(String), sizeof(JsonValue),
                                  murmurhash3_str, str_cmp,
                                  &str_val_ops, &json_val_ops);
    return v;
}

// Array / object helpers

void json_array_push(JsonValue* arr, JsonValue** elem)
{
    CHECK_FATAL(arr->type != JSON_ARRAY, "not an array");
    CHECK_FATAL(!elem || !*elem, "null elem");
    genVec_push_move(arr->array, (u8**)elem);
}

u64 json_array_len(const JsonValue* arr)
{
    CHECK_FATAL(arr->type != JSON_ARRAY, "not an array");
    return genVec_size(arr->array);
}

JsonValue* json_array_get(const JsonValue* arr, u64 i)
{
    CHECK_FATAL(arr->type != JSON_ARRAY, "not an array");
    return (JsonValue*)genVec_get_ptr_mut(arr->array, i);
}

void json_object_set(JsonValue* obj, const char* key, JsonValue** val)
{
    CHECK_FATAL(obj->type != JSON_OBJECT, "not an object");
    CHECK_FATAL(!val || !*val, "null val");
    String key_str;
    string_create_stk(&key_str, key);
    hashmap_put_val_move(obj->object, (const u8*)&key_str, (u8**)val);
    string_destroy_stk(&key_str);
}

JsonValue* json_object_get(JsonValue* obj, const char* key)
{
    CHECK_FATAL(obj->type != JSON_OBJECT, "not an object");
    String key_str;
    string_create_stk(&key_str, key);
    u8* ptr = hashmap_get_ptr(obj->object, (const u8*)&key_str);
    string_destroy_stk(&key_str);
    return (JsonValue*)ptr;
}

b8 json_object_has(const JsonValue* obj, const char* key)
{
    CHECK_FATAL(obj->type != JSON_OBJECT, "not an object");
    String key_str;
    string_create_stk(&key_str, key);
    b8 found = hashmap_has(obj->object, (const u8*)&key_str);
    string_destroy_stk(&key_str);
    return found;
}

// Lexer

typedef enum {
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COLON,
    TOK_COMMA,
    TOK_STRING,
    TOK_NUMBER,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NULL,
    TOK_EOF,
    TOK_ERROR,
} TokenType;

typedef struct {
    TokenType   type;
    const char* start;
    u64         len;
} Token;

typedef struct {
    const char* src;
    u64         pos;
    u64         len;
    genVec*     tokens;
} Lexer;

static b8 lex_string(Lexer* lex)
{
    u64 start = lex->pos++;
    while (lex->pos < lex->len) {
        char c = lex->src[lex->pos];
        if (c == '\\') {
            lex->pos += 2;
            continue;
        }
        if (c == '"') {
            lex->pos++;
            Token tok = {TOK_STRING, lex->src + start + 1, lex->pos - start - 2};
            VEC_PUSH(lex->tokens, tok);
            return true;
        }
        lex->pos++;
    }

    WARN("lex error: unterminated string\n");
    return false;
}

static b8 lex_number(Lexer* lex)
{
    u64 start = lex->pos;
    if (lex->src[lex->pos] == '-') {
        lex->pos++;
    }
    while (lex->pos < lex->len && isdigit((unsigned char)lex->src[lex->pos])) {
        lex->pos++;
    }
    if (lex->pos < lex->len && lex->src[lex->pos] == '.') {
        lex->pos++;
        while (lex->pos < lex->len && isdigit((unsigned char)lex->src[lex->pos])) {
            lex->pos++;
        }
    }
    if (lex->pos < lex->len && (lex->src[lex->pos] == 'e' || lex->src[lex->pos] == 'E')) {
        lex->pos++;
        if (lex->pos < lex->len && (lex->src[lex->pos] == '+' || lex->src[lex->pos] == '-')) {
            lex->pos++;
        }
        while (lex->pos < lex->len && isdigit((unsigned char)lex->src[lex->pos])) {
            lex->pos++;
        }
    }

    Token tok = {TOK_NUMBER, lex->src + start, lex->pos - start};
    VEC_PUSH(lex->tokens, tok);
    return true;
}

static b8 lex_keyword(Lexer* lex, const char* kw, TokenType type)
{
    u64 kwlen = strlen(kw);
    if (lex->pos + kwlen > lex->len || memcmp(lex->src + lex->pos, kw, kwlen) != 0) {
        return false;
    }

    Token tok = {type, lex->src + lex->pos, kwlen};
    lex->pos += kwlen;
    VEC_PUSH(lex->tokens, tok);
    return true;
}

static b8 do_lex(Lexer* lex)
{
    while (lex->pos < lex->len) {
        char c = lex->src[lex->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            lex->pos++;
            continue;
        }
        switch (c) {
        case '{': {
            Token t = {TOK_LBRACE, lex->src + lex->pos, 1};
            VEC_PUSH(lex->tokens, t);
            lex->pos++;
            break;
        }
        case '}': {
            Token t = {TOK_RBRACE, lex->src + lex->pos, 1};
            VEC_PUSH(lex->tokens, t);
            lex->pos++;
            break;
        }
        case '[': {
            Token t = {TOK_LBRACKET, lex->src + lex->pos, 1};
            VEC_PUSH(lex->tokens, t);
            lex->pos++;
            break;
        }
        case ']': {
            Token t = {TOK_RBRACKET, lex->src + lex->pos, 1};
            VEC_PUSH(lex->tokens, t);
            lex->pos++;
            break;
        }
        case ':': {
            Token t = {TOK_COLON, lex->src + lex->pos, 1};
            VEC_PUSH(lex->tokens, t);
            lex->pos++;
            break;
        }
        case ',': {
            Token t = {TOK_COMMA, lex->src + lex->pos, 1};
            VEC_PUSH(lex->tokens, t);
            lex->pos++;
            break;
        }
        case '"':
            if (!lex_string(lex)) {
                return false;
            }
            break;
        case 't':
            if (!lex_keyword(lex, "true", TOK_TRUE)) {
                WARN("expected 'true'\n");
                return false;
            }
            break;
        case 'f':
            if (!lex_keyword(lex, "false", TOK_FALSE)) {
                WARN("expected 'false'\n");
                return false;
            }
            break;
        case 'n':
            if (!lex_keyword(lex, "null", TOK_NULL)) {
                WARN("expected 'null'\n");
                return false;
            }
            break;
        default:
            if (c == '-' || isdigit((unsigned char)c)) {
                if (!lex_number(lex)) {
                    return false;
                }
            } else {
                WARN("unexpected char '%c'\n", c);
                return false;
            }
            break;
        }
    }

    Token eof = {TOK_EOF, NULL, 0};
    VEC_PUSH(lex->tokens, eof);
    return true;
}

// Recursive-descent parser

typedef struct {
    genVec* tokens;
    u64     pos;
} Parser;

static Token* ppeek(Parser* p)
{
    return VEC_AT_MUT(p->tokens, Token, p->pos);
}
static Token* pconsume(Parser* p)
{
    return VEC_AT_MUT(p->tokens, Token, p->pos++);
}

static b8 pexpect(Parser* p, TokenType t, const char* ctx)
{
    if (ppeek(p)->type == t) {
        p->pos++;
        return true;
    }
    WARN("parse error in %s: unexpected token\n", ctx);
    return false;
}


static JsonValue* parse_value(Parser* p);


static JsonValue* parse_string_tok(Parser* p)
{
    Token*     t = pconsume(p);
    JsonValue* v = malloc(sizeof(JsonValue));
    v->type      = JSON_STRING;
    string_create_stk(&v->string, NULL);
    string_reserve(&v->string, t->len + 1);
    for (u64 i = 0; i < t->len; i++) {
        char c = t->start[i];
        if (c == '\\' && i + 1 < t->len) {
            i++;
            switch (t->start[i]) {
            case '"':
                string_append_char(&v->string, '"');
                break;
            case '\\':
                string_append_char(&v->string, '\\');
                break;
            case '/':
                string_append_char(&v->string, '/');
                break;
            case 'n':
                string_append_char(&v->string, '\n');
                break;
            case 'r':
                string_append_char(&v->string, '\r');
                break;
            case 't':
                string_append_char(&v->string, '\t');
                break;
            case 'b':
                string_append_char(&v->string, '\b');
                break;
            case 'f':
                string_append_char(&v->string, '\f');
                break;
            case 'u':
                string_append_char(&v->string, '?');
                if (i + 4 < t->len) {
                    i += 4;
                }
                break;
            default:
                string_append_char(&v->string, t->start[i]);
                break;
            }
        } else {
            string_append_char(&v->string, c);
        }
    }
    return v;
}

static JsonValue* parse_number(Parser* p)
{
    Token* t = pconsume(p);
    char   buf[64];
    u64    n = t->len < 63 ? t->len : 63;
    memcpy(buf, t->start, n);
    buf[n] = '\0';
    return json_number(strtod(buf, NULL));
}

static JsonValue* parse_array(Parser* p)
{
    pconsume(p);
    JsonValue* arr = json_array_new();
    if (ppeek(p)->type == TOK_RBRACKET) {
        pconsume(p);
        return arr;
    }
    while (1) {
        JsonValue* elem = parse_value(p);
        if (!elem) {
            json_value_destroy(arr);
            return NULL;
        }
        json_array_push(arr, &elem);
        if (ppeek(p)->type == TOK_RBRACKET) {
            pconsume(p);
            break;
        }
        if (ppeek(p)->type != TOK_COMMA) {
            WARN("parse error: expected ',' or ']'\n");
            json_value_destroy(arr);
            return NULL;
        }
        pconsume(p);
    }
    return arr;
}

static JsonValue* parse_object(Parser* p)
{
    pconsume(p);
    JsonValue* obj = json_object_new();
    if (ppeek(p)->type == TOK_RBRACE) {
        pconsume(p);
        return obj;
    }
    while (1) {
        if (ppeek(p)->type != TOK_STRING) {
            WARN("parse error: expected string key\n");
            json_value_destroy(obj);
            return NULL;
        }
        Token* key_tok = pconsume(p);
        char   key_buf[1024];
        u64    klen = key_tok->len < 1023 ? key_tok->len : 1023;
        memcpy(key_buf, key_tok->start, klen);
        key_buf[klen] = '\0';
        if (!pexpect(p, TOK_COLON, "object")) {
            json_value_destroy(obj);
            return NULL;
        }
        JsonValue* val = parse_value(p);
        if (!val) {
            json_value_destroy(obj);
            return NULL;
        }
        json_object_set(obj, key_buf, &val);
        if (ppeek(p)->type == TOK_RBRACE) {
            pconsume(p);
            break;
        }
        if (ppeek(p)->type != TOK_COMMA) {
            WARN("parse error: expected ',' or '}'\n");
            json_value_destroy(obj);
            return NULL;
        }
        pconsume(p);
    }
    return obj;
}

static JsonValue* parse_value(Parser* p)
{
    switch (ppeek(p)->type) {
    case TOK_NULL:
        pconsume(p);
        return json_null();
    case TOK_TRUE:
        pconsume(p);
        return json_bool(true);
    case TOK_FALSE:
        pconsume(p);
        return json_bool(false);
    case TOK_NUMBER:
        return parse_number(p);
    case TOK_STRING:
        return parse_string_tok(p);
    case TOK_LBRACKET:
        return parse_array(p);
    case TOK_LBRACE:
        return parse_object(p);
    default:
        WARN("parse error: unexpected token type %d\n", ppeek(p)->type);
        return NULL;
    }
}

JsonValue* json_parse(const char* input)
{
    if (!input) {
        return NULL;
    }
    Lexer lex = {
        .src    = input,
        .pos    = 0,
        .len    = strlen(input),
        .tokens = genVec_init(64, sizeof(Token), NULL)
    };
    if (!do_lex(&lex)) {
        genVec_destroy(lex.tokens);
        return NULL;
    }
    Parser     parser = {.tokens = lex.tokens, .pos = 0};
    JsonValue* root   = parse_value(&parser);
    if (root && ppeek(&parser)->type != TOK_EOF) {
        WARN("parse error: trailing content\n");
        json_value_destroy(root);
        root = NULL;
    }
    genVec_destroy(lex.tokens);
    return root;
}


// Output

/*
 * Returns true if d can be represented exactly as a long long
 * (i.e. it is finite, has no fractional part, and fits in the range).
 */
static b8 is_integer(double d)
{
    return isfinite(d) && d == floor(d) &&
           d >= (double)LLONG_MIN && d <= (double)LLONG_MAX;
}

static void print_indent(int depth, int width)
{
    for (int i = 0; i < depth * width; i++) {
        putchar(' ');
    }
}

static void print_str_esc(const String* s)
{
    putchar('"');
    for (u64 i = 0; i < s->size; i++) {
        char c = ((const char*)s->data)[i];
        switch (c) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n",  stdout); break;
        case '\r': fputs("\\r",  stdout); break;
        case '\t': fputs("\\t",  stdout); break;
        default:   putchar(c);            break;
        }
    }
    putchar('"');
}

static void print_val(const JsonValue* val, int depth, int width)
{
    switch (val->type) {
    case JSON_NULL:
        fputs("null", stdout);
        break;
    case JSON_BOOL:
        fputs(val->boolean ? "true" : "false", stdout);
        break;
    case JSON_NUMBER:
        /* FIX: was `val->number == (double)val->number` — always true.
         * Correct check: does the value round-trip through long long? */
        if (is_integer(val->number)) {
            printf("%lld", (long long)val->number);
        } else {
            printf("%.10g", val->number);
        }
        break;
    case JSON_STRING:
        print_str_esc(&val->string);
        break;
    case JSON_ARRAY: {
        u64 n = genVec_size(val->array);
        if (n == 0) {
            fputs("[]", stdout);
            break;
        }
        fputs("[\n", stdout);
        VEC_FOREACH(val->array, JsonValue, elem) {
            u64 idx = (u64)((u8*)elem - val->array->data) / sizeof(JsonValue);
            print_indent(depth + 1, width);
            print_val(elem, depth + 1, width);
            if (idx + 1 < n) { putchar(','); }
            putchar('\n');
        }
        print_indent(depth, width);
        putchar(']');
        break;
    }
    case JSON_OBJECT: {
        u64 n = hashmap_size(val->object);
        if (n == 0) {
            fputs("{}", stdout);
            break;
        }
        fputs("{\n", stdout);
        u64 printed = 0;
        /* FIX: was raw KV_p shadow-struct cast — fragile and bypassed the
         * proper API. Use MAP_FOREACH_BUCKET instead. */
        MAP_FOREACH_BUCKET(val->object, kv) {
            const String*    k = (const String*)kv->key;
            const JsonValue* v = (const JsonValue*)kv->val;
            print_indent(depth + 1, width);
            print_str_esc(k);
            fputs(": ", stdout);
            print_val(v, depth + 1, width);
            printed++;
            if (printed < n) { putchar(','); }
            putchar('\n');
        }
        print_indent(depth, width);
        putchar('}');
        break;
    }
    }
}

void json_print(const JsonValue* val, int indent_width)
{
    if (!val) {
        fputs("(null)\n", stdout);
        return;
    }
    print_val(val, 0, indent_width);
    putchar('\n');
}

static void serialize_str_esc(const String* s, String* out)
{
    string_append_char(out, '"');
    for (u64 i = 0; i < s->size; i++) {
        char c = ((const char*)s->data)[i];
        switch (c) {
        case '"':  string_append_cstr(out, "\\\""); break;
        case '\\': string_append_cstr(out, "\\\\"); break;
        case '\n': string_append_cstr(out, "\\n");  break;
        case '\r': string_append_cstr(out, "\\r");  break;
        case '\t': string_append_cstr(out, "\\t");  break;
        default:   string_append_char(out, c);       break;
        }
    }
    string_append_char(out, '"');
}

static void serialize_val(const JsonValue* val, String* out)
{
    switch (val->type) {
    case JSON_NULL:
        string_append_cstr(out, "null");
        break;
    case JSON_BOOL:
        string_append_cstr(out, val->boolean ? "true" : "false");
        break;
    case JSON_NUMBER: {
        char buf[32];
        /* FIX: same is_integer() guard as print_val */
        if (is_integer(val->number)) {
            snprintf(buf, sizeof(buf), "%lld", (long long)val->number);
        } else {
            snprintf(buf, sizeof(buf), "%.10g", val->number);
        }
        string_append_cstr(out, buf);
        break;
    }
    case JSON_STRING:
        serialize_str_esc(&val->string, out);
        break;
    case JSON_ARRAY: {
        string_append_char(out, '[');
        u64 n = genVec_size(val->array);
        for (u64 i = 0; i < n; i++) {
            if (i > 0) { string_append_char(out, ','); }
            serialize_val((const JsonValue*)genVec_get_ptr(val->array, i), out);
        }
        string_append_char(out, ']');
        break;
    }
    case JSON_OBJECT: {
        string_append_char(out, '{');
        b8 first = true;
        /* FIX: was raw KV_s shadow-struct cast — use MAP_FOREACH_BUCKET. */
        MAP_FOREACH_BUCKET(val->object, kv) {
            if (!first) { string_append_char(out, ','); }
            first = false;
            serialize_str_esc((const String*)kv->key, out);
            string_append_char(out, ':');
            serialize_val((const JsonValue*)kv->val, out);
        }
        string_append_char(out, '}');
        break;
    }
    }
}

String* json_to_string(const JsonValue* val)
{
    String* out = string_create();
    if (val) {
        serialize_val(val, out);
    }
    return out;
}
