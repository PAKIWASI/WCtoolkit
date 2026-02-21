#include "json_parser.h"
#include "String.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ════════════════════════════════════════════════════════════════════════════
 * PARSER STATE
 *
 * All parse functions share a cursor that walks the raw text.
 * We keep it as a simple index into the String's data — no pointer aliasing.
 * ════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char* src;   // pointer into String data (NOT null-terminated)
    u64         len;   // total source length
    u64         pos;   // current read position
    Arena*      arena; // arena for all json_val / json_pair allocations
} Parser;


// CREATION / DESTRUCTION

json_val* json_val_create(Arena* arena, json_type type)
{
    json_val* val = (json_val*)arena_alloc(arena, sizeof(json_val));
    CHECK_FATAL(!val, "arena exhausted allocating json_val");

    switch (type) {
        case J_NUM:
            val->type    = J_NUM;
            val->num_val = 0.0;
            break;
        case J_BOOL:
            val->type     = J_BOOL;
            val->bool_val = false;
            break;
        case J_STR:
            val->type    = J_STR;
            val->str_val = string_create();
            break;
        case J_OBJ:
            val->type              = J_OBJ;
            val->obj_val.pairs     = json_pair_create(arena);
            val->obj_val.count     = 0;
            break;
        case J_ARR:
            val->type          = J_ARR;
            val->arr_val.items = (json_val**)arena_alloc(arena, sizeof(json_val*) * 8);
            val->arr_val.count = 0;
            val->arr_val.cap   = 8;
            break;
        case J_NULL:
            val->type = J_NULL;
            break;
    }

    return val;
}


void json_val_destroy(json_val* val)
{
    if (!val) { return; }

    switch (val->type) {
        case J_STR:
            string_destroy(val->str_val);
            break;
        case J_OBJ:
            for (u32 i = 0; i < val->obj_val.count; i++) {
                json_pair* p = &val->obj_val.pairs[i];
                string_destroy_stk(&p->key);
                json_val_destroy(&p->val);
            }
            break;
        case J_ARR:
            for (u32 i = 0; i < val->arr_val.count; i++) {
                json_val_destroy(val->arr_val.items[i]);
            }
            break;
        case J_NUM:
        case J_BOOL:
        case J_NULL:
            break;
    }

    // Note: json_val itself is arena-allocated — we don't free it here.
    // Caller frees by releasing the arena.
}


json_pair* json_pair_create(Arena* arena)
{
    // Start with capacity for 8 pairs; we'll realloc-style grow with malloc
    // when the arena can't do variable-size growth. For simplicity we use
    // malloc for the pair array since its size is unknown at parse time.
    // This is the only heap allocation outside the arena.
    json_pair* pairs = (json_pair*)malloc(sizeof(json_pair) * 8);
    CHECK_FATAL(!pairs, "malloc failed for json_pair array");
    (void)arena; // reserved for future arena-only strategy
    return pairs;
}



String* load_json_file(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    CHECK_WARN_RET(!f, NULL, "could not open file: %s", filename);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        WARN("file is empty or unreadable: %s", filename);
        return NULL;
    }

    String* s = string_create();
    string_reserve(s, (u64)size);

    // Read directly into the string by appending each byte
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            string_append_char(s, buf[i]);
        }
    }

    fclose(f);
    return s;
}



static inline b8 p_done(const Parser* p)
{
    return p->pos >= p->len;
}

static inline char p_peek(const Parser* p)
{
    if (p_done(p)) { return '\0'; }
    return p->src[p->pos];
}

static inline char p_advance(Parser* p)
{
    if (p_done(p)) { return '\0'; }
    return p->src[p->pos++];
}

static inline void p_skip_spaces(Parser* p)
{
    while (!p_done(p)) {
        char c = p_peek(p);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p->pos++;
        } else {
            break;
        }
    }
}

// Consume a specific expected character, return false on mismatch
static b8 p_expect(Parser* p, char expected)
{
    p_skip_spaces(p);
    if (p_done(p) || p_peek(p) != expected) {
        WARN("expected '%c' at pos %llu, got '%c'",
             expected, (unsigned long long)p->pos,
             p_done(p) ? '?' : p_peek(p));
        return false;
    }
    p->pos++;
    return true;
}

// Consume a literal keyword ("true", "false", "null")
static b8 p_expect_literal(Parser* p, const char* lit)
{
    u64 len = (u64)strlen(lit);
    if (p->pos + len > p->len) { return false; }
    if (strncmp(p->src + p->pos, lit, len) != 0) { return false; }
    p->pos += len;
    return true;
}


/* ════════════════════════════════════════════════════════════════════════════
 * READERS — each advances the parser cursor and returns the parsed value
 * ════════════════════════════════════════════════════════════════════════════ */

// Reads a JSON string value (expects opening quote already consumed by caller,
// OR reads from current position if opening quote not yet consumed).
static String* p_read_string(Parser* p)
{
    if (!p_expect(p, '"')) { return NULL; }

    String* s = string_create();

    while (!p_done(p)) {
        char c = p_advance(p);

        if (c == '"') {
            // End of string
            return s;
        }

        if (c == '\\') {
            // Escape sequence
            if (p_done(p)) { break; }
            char esc = p_advance(p);
            switch (esc) {
                case '"':  string_append_char(s, '"');  break;
                case '\\': string_append_char(s, '\\'); break;
                case '/':  string_append_char(s, '/');  break;
                case 'n':  string_append_char(s, '\n'); break;
                case 'r':  string_append_char(s, '\r'); break;
                case 't':  string_append_char(s, '\t'); break;
                case 'b':  string_append_char(s, '\b'); break;
                case 'f':  string_append_char(s, '\f'); break;
                case 'u': {
                    // \uXXXX — read 4 hex digits, basic BMP-only support
                    if (p->pos + 4 > p->len) { break; }
                    char hex[5] = {0};
                    memcpy(hex, p->src + p->pos, 4);
                    p->pos += 4;
                    unsigned codepoint = 0;
                    sscanf(hex, "%x", &codepoint);
                    // Encode to UTF-8 (BMP only)
                    if (codepoint < 0x80) {
                        string_append_char(s, (char)codepoint);
                    } else if (codepoint < 0x800) {
                        string_append_char(s, (char)(0xC0 | (codepoint >> 6)));
                        string_append_char(s, (char)(0x80 | (codepoint & 0x3F)));
                    } else {
                        string_append_char(s, (char)(0xE0 | (codepoint >> 12)));
                        string_append_char(s, (char)(0x80 | ((codepoint >> 6) & 0x3F)));
                        string_append_char(s, (char)(0x80 | (codepoint & 0x3F)));
                    }
                    break;
                }
                default:
                    // Unknown escape: keep as-is
                    string_append_char(s, esc);
                    break;
            }
        } else {
            string_append_char(s, c);
        }
    }

    WARN("unterminated string");
    string_destroy(s);
    return NULL;
}

static double p_read_number(Parser* p)
{
    p_skip_spaces(p);
    const char* start = p->src + p->pos;
    char* end;

    double val = strtod(start, &end);

    if (end == start) {
        WARN("expected number at pos %llu", (unsigned long long)p->pos);
        return 0.0;
    }

    p->pos += (u64)(end - start);
    return val;
}

static b8 p_read_bool(Parser* p)
{
    p_skip_spaces(p);
    if (p_peek(p) == 't') {
        if (p_expect_literal(p, "true")) { return true; }
    } else {
        if (p_expect_literal(p, "false")) { return false; }
    }
    WARN("expected bool at pos %llu", (unsigned long long)p->pos);
    return false;
}

// Forward declaration — read_value and read_object are mutually recursive
static json_val* p_read_value(Parser* p);

static json_val* p_read_array(Parser* p)
{
    if (!p_expect(p, '[')) { return NULL; }

    json_val* arr_val   = json_val_create(p->arena, J_ARR);
    u32       cap       = arr_val->arr_val.cap;

    p_skip_spaces(p);
    if (p_peek(p) == ']') { p->pos++; return arr_val; }  // empty array

    while (!p_done(p)) {
        json_val* item = p_read_value(p);
        if (!item) { break; }

        // Grow item array if needed (malloc-based since arena can't realloc)
        if (arr_val->arr_val.count >= cap) {
            cap *= 2;
            arr_val->arr_val.items = (json_val**)realloc(
                arr_val->arr_val.items, sizeof(json_val*) * cap);
            CHECK_FATAL(!arr_val->arr_val.items, "realloc failed for array items");
            arr_val->arr_val.cap = cap;
        }

        arr_val->arr_val.items[arr_val->arr_val.count++] = item;

        p_skip_spaces(p);
        if (p_peek(p) == ']') { p->pos++; break; }
        if (!p_expect(p, ',')) { break; }
    }

    return arr_val;
}

static json_val* p_read_object(Parser* p)
{
    if (!p_expect(p, '{')) { return NULL; }

    json_val* obj = json_val_create(p->arena, J_OBJ);
    u32       cap = 8;   // initial pair capacity (json_pair_create gives 8)

    p_skip_spaces(p);
    if (p_peek(p) == '}') { p->pos++; return obj; }   // empty object

    while (!p_done(p)) {
        p_skip_spaces(p);

        // Read key
        String* key = p_read_string(p);
        if (!key) { break; }

        if (!p_expect(p, ':')) {
            string_destroy(key);
            break;
        }

        // Read value
        json_val* val_item = p_read_value(p);
        if (!val_item) {
            string_destroy(key);
            break;
        }

        // Grow pair array if needed
        if (obj->obj_val.count >= cap) {
            cap *= 2;
            obj->obj_val.pairs = (json_pair*)realloc(
                obj->obj_val.pairs, sizeof(json_pair) * cap);
            CHECK_FATAL(!obj->obj_val.pairs, "realloc failed for pair array");
        }

        // Store pair — key by value (String inline), val by value (json_val inline)
        json_pair* pair = &obj->obj_val.pairs[obj->obj_val.count++];
        string_create_stk(&pair->key, "");   // init the inline String
        string_append_string(&pair->key, key);
        string_destroy(key);
        pair->val = *val_item;               // copy the val struct (shallow ok for arena'd val)

        p_skip_spaces(p);
        if (p_peek(p) == '}') { p->pos++; break; }
        if (!p_expect(p, ',')) { break; }
    }

    return obj;
}

static json_val* p_read_value(Parser* p)
{
    p_skip_spaces(p);
    if (p_done(p)) { return NULL; }

    char c = p_peek(p);

    switch (c) {
        case '"': {
            json_val* v = json_val_create(p->arena, J_STR);
            string_destroy(v->str_val);   // replace with our parsed string
            v->str_val = p_read_string(p);
            return v;
        }
        case '{':
            return p_read_object(p);

        case '[':
            return p_read_array(p);

        case 't':
        case 'f': {
            json_val* v = json_val_create(p->arena, J_BOOL);
            v->bool_val = p_read_bool(p);
            return v;
        }
        case 'n': {
            if (p_expect_literal(p, "null")) {
                return json_val_create(p->arena, J_NULL);
            }
            WARN("expected 'null' at pos %llu", (unsigned long long)p->pos);
            return NULL;
        }
        default: {
            // Must be a number (possibly negative)
            if (c == '-' || (c >= '0' && c <= '9')) {
                json_val* v = json_val_create(p->arena, J_NUM);
                v->num_val  = p_read_number(p);
                return v;
            }
            WARN("unexpected character '%c' at pos %llu",
                 c, (unsigned long long)p->pos);
            return NULL;
        }
    }
}



void skip_spaces(const String* raw_text)
{
    // The public-facing version has no cursor concept.
    // Useful only if called with a mutable context externally.
    // Most users will use the parser-internal p_skip_spaces.
    (void)raw_text;
    WARN("skip_spaces: use the parse API (json_parse) instead of calling directly");
}

String* read_string(const String* raw_text)
{
    Parser p = {
        .src   = string_data_ptr((String*)raw_text),
        .len   = string_len(raw_text),
        .pos   = 0,
        .arena = NULL,   // not needed for string-only parse
    };
    return p_read_string(&p);
}

double read_number(const String* raw_text)
{
    Parser p = {
        .src   = string_data_ptr((String*)raw_text),
        .len   = string_len(raw_text),
        .pos   = 0,
        .arena = NULL,
    };
    return p_read_number(&p);
}

b8 read_bool(const String* raw_text)
{
    Parser p = {
        .src   = string_data_ptr((String*)raw_text),
        .len   = string_len(raw_text),
        .pos   = 0,
        .arena = NULL,
    };
    return p_read_bool(&p);
}

json_val* read_object(const String* raw_text)
{
    // Creates its own scratch arena — caller owns the returned json_val tree
    // and must call json_val_destroy + free the arena themselves.
    // Recommended: use json_parse() instead which bundles both.
    Arena* arena = arena_create(nMB(4));
    Parser p = {
        .src   = string_data_ptr((String*)raw_text),
        .len   = string_len(raw_text),
        .pos   = 0,
        .arena = arena,
    };
    return p_read_object(&p);
}

json_val* read_value(const String* raw_text)
{
    Arena* arena = arena_create(nMB(4));
    Parser p = {
        .src   = string_data_ptr((String*)raw_text),
        .len   = string_len(raw_text),
        .pos   = 0,
        .arena = arena,
    };
    return p_read_value(&p);
}


/* ════════════════════════════════════════════════════════════════════════════
 * MAIN ENTRY POINT
 * ════════════════════════════════════════════════════════════════════════════ */

json_val* json_parse(const String* raw_text, Arena* arena)
{
    CHECK_FATAL(!raw_text, "raw_text is null");
    CHECK_FATAL(!arena,    "arena is null");

    Parser p = {
        .src   = string_data_ptr((String*)raw_text),
        .len   = string_len(raw_text),
        .pos   = 0,
        .arena = arena,
    };

    return p_read_value(&p);
}

json_val* json_parse_file(const char* filename, Arena* arena)
{
    String* raw = load_json_file(filename);
    if (!raw) { return NULL; }

    json_val* root = json_parse(raw, arena);
    string_destroy(raw);
    return root;
}


/* ════════════════════════════════════════════════════════════════════════════
 * PATH-BASED LOOKUP
 *
 * get_value(root, "details.name") walks the object tree following dot-separated
 * keys, also supports array indexing like "items.0.price".
 * ════════════════════════════════════════════════════════════════════════════ */

json_val* get_value(json_val* root, const String* path)
{
    CHECK_FATAL(!root, "root is null");
    CHECK_FATAL(!path, "path is null");

    json_val* cur = root;
    u64       len = string_len(path);
    u64       i   = 0;

    while (i < len && cur) {
        // Find next segment (up to '.' or end of path)
        u64 j = i;
        while (j < len && string_char_at(path, j) != '.') { j++; }

        u64 seg_len = j - i;
        if (seg_len == 0) { i = j + 1; continue; }

        // Extract segment as a small cstr on the stack
        char seg[256];
        if (seg_len >= sizeof(seg)) {
            WARN("path segment too long");
            return NULL;
        }
        for (u64 k = 0; k < seg_len; k++) {
            seg[k] = string_char_at(path, i + k);
        }
        seg[seg_len] = '\0';

        if (cur->type == J_OBJ) {
            // Find matching key
            b8 found = false;
            for (u32 p_i = 0; p_i < cur->obj_val.count; p_i++) {
                json_pair* pair = &cur->obj_val.pairs[p_i];
                if (string_equals_cstr(&pair->key, seg)) {
                    cur   = &pair->val;
                    found = true;
                    break;
                }
            }
            if (!found) {
                WARN("key not found: %s", seg);
                return NULL;
            }
        } else if (cur->type == J_ARR) {
            // Parse segment as integer index
            char* endptr;
            long idx = strtol(seg, &endptr, 10);
            if (endptr == seg || idx < 0 || (u32)idx >= cur->arr_val.count) {
                WARN("invalid array index: %s", seg);
                return NULL;
            }
            cur = cur->arr_val.items[idx];
        } else {
            WARN("cannot descend into non-object/array at segment: %s", seg);
            return NULL;
        }

        i = j + 1;  // skip past the '.'
    }

    return cur;
}


/* ════════════════════════════════════════════════════════════════════════════
 * PRINTING
 * ════════════════════════════════════════════════════════════════════════════ */

static void print_value_indent(const json_val* value, int indent);

static void print_indent(int indent)
{
    for (int i = 0; i < indent * 2; i++) { putchar(' '); }
}

static void print_value_indent(const json_val* value, int indent)
{
    if (!value) { printf("(null)\n"); return; }

    switch (value->type) {
        case J_NULL:
            printf("null");
            break;

        case J_BOOL:
            printf("%s", value->bool_val ? "true" : "false");
            break;

        case J_NUM:
            // Print without trailing zeros when whole number
            if (value->num_val == (long long)value->num_val) {
                printf("%lld", (long long)value->num_val);
            } else {
                printf("%g", value->num_val);
            }
            break;

        case J_STR:
            printf("\"");
            TEMP_CSTR_READ(value->str_val) {
                printf("%s", string_data_ptr(value->str_val));
            }
            printf("\"");
            break;

        case J_OBJ:
            printf("{\n");
            for (u32 i = 0; i < value->obj_val.count; i++) {
                const json_pair* pair = &value->obj_val.pairs[i];
                print_indent(indent + 1);
                printf("\"");
                TEMP_CSTR_READ(&pair->key) {
                    printf("%s", string_data_ptr(&pair->key));
                }
                printf("\": ");
                print_value_indent(&pair->val, indent + 1);
                if (i + 1 < value->obj_val.count) { printf(","); }
                printf("\n");
            }
            print_indent(indent);
            printf("}");
            break;

        case J_ARR:
            printf("[\n");
            for (u32 i = 0; i < value->arr_val.count; i++) {
                print_indent(indent + 1);
                print_value_indent(value->arr_val.items[i], indent + 1);
                if (i + 1 < value->arr_val.count) { printf(","); }
                printf("\n");
            }
            print_indent(indent);
            printf("]");
            break;
    }
}

void print_value(const json_val* value)
{
    print_value_indent(value, 0);
    printf("\n");
}
