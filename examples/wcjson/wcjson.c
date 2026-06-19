#include "wcjson.h"
#include "arena.h"
#include "common.h"

#define JSMN_PARENT_LINKS
#include "jsmn.h"

#include <stdio.h>



// reads entire json file into a buffer (null terminated)
static int read_file(const char* filename, char** output, Arena* arena)
{
    FILE* f = fopen(filename, "rb");
    if (!f) {
        WARN("cannot open '%s'", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        WARN("file '%s' empty", filename);
        fclose(f);
        return NULL;
    }

    // char* buf = malloc((size_t)sz + 1);
    char* buf = ARENA_ALLOC_N(arena, char, (u64)sz+1);
    if (!buf) {
        WARN("OOM reading '%s'", filename);
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    CHECK_WARN_RET((long)n != sz, -1, "short read on '%s'", filename);

    buf[sz] = '\0';
    *output = buf;
    return (int)sz;
}


// parse json (2 phases) and build token arr
static int jsmn_parse_json(char* json_buf, u32 json_len, jsmntok_t** tokens)    // tokens allocatedon arena
{
    // Two-pass jsmn parse
    // Pass 1: tokens=NULL → jsmn counts tokens, returns how many
    jsmn_parser p;
    jsmn_init(&p);

    int n_tokens = jsmn_parse(&p, json_buf, json_len, NULL, 0);
    CHECK_WARN_RET(n_tokens <= 0, NULL, "jsmn count pass failed: %d", n_tokens);

    // Pass 2: allocate token array, parse for real
    jsmntok_t* toks = malloc((size_t)n_tokens * sizeof(jsmntok_t));
    CHECK_WARN_RET(!toks, NULL, "OOM tokens");

    // have to re-init
    jsmn_init(&p);
    // build tokens
    int r = jsmn_parse(&p, json_buf, json_len, toks, (u32)n_tokens);
    CHECK_WARN_RET(r < 0, -1, "jsmn second pass failed");

    // this has to be true, otherwise, invalid json
    if (toks[0].type != JSMN_OBJECT) {
        free(toks);
        WARN("JSON root is not an object");
        return NULL;
    }

    *tokens = toks;
    return n_tokens;
}


wcjson_val wcjson_create(void)
{
    wcjson_val val;
    val.map  = hashmap_create();
    val.type = WCJSON_OBJECT;

}




