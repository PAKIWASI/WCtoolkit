#define JSMN_PARENT_LINKS
#include "jsmn.h"
#include "wcjson.h"
#include "arena.h"
#include "common.h"

#include <stdio.h>




static long read_file(const char* filename, char** output);
static int jsmn_parse_json(char* json_buf, u32 json_len, jsmntok_t** tokens);
static bool build_json_tree(char* raw_json, jsmntok_t* tokens, wcjson* json);



// === API ===


wcjson* wcjson_create_from_file(const char* filename)
{
    // processing raw json file with jsmn

    char* raw_json;
    long size = read_file(filename, &raw_json);

    // NOTE: we assume that if raw json takes `size` bytes then wcjson
    // overhead will be double that. we need this to initiate arena
    // we allocate arena with size * 2 as a safety margin (i hope it'll be enough!)
    // Maybe we can alocate by pages?

    wcjson* json = malloc(sizeof(wcjson));
    CHECK_FATAL(!json, "wcjson malloc failed");
    arena_create_stk(&json->arena, (u64)size * 2);

    jsmntok_t* tokens;
    int num_tokens = jsmn_parse_json(raw_json, (u32)size, &tokens);
    CHECK_FATAL(num_tokens != -1, "jsmn parse failure");

    // using jsmn tokens to create the json tree structure



    free(raw_json);
    return json;
}




// reads entire json file into a buffer (null terminated)
static long read_file(const char* filename, char** output)
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

    char* buf = malloc((size_t)sz + 1);
    if (!buf) {
        WARN("OOM reading '%s'", filename);
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if ((long)n != sz) {
        free(buf);
        WARN("short read on '%s'", filename);
        return -1;
    }

    buf[sz] = '\0';
    *output = buf;
    return sz;
}


// parse json (2 phases) and build token arr
static int jsmn_parse_json(char* json_buf, u32 json_len, jsmntok_t** tokens)    // tokens allocatedon arena
{
    // Two-pass jsmn parse
    // Pass 1: tokens=NULL → jsmn counts tokens, returns how many
    jsmn_parser p;
    jsmn_init(&p);

    int n_tokens = jsmn_parse(&p, json_buf, json_len, NULL, 0);
    CHECK_WARN_RET(n_tokens <= 0, -1, "jsmn count pass failed: %d", n_tokens);

    // Pass 2: allocate token array, parse for real
    jsmntok_t* toks = malloc((size_t)n_tokens * sizeof(jsmntok_t));
    CHECK_WARN_RET(!toks, -1, "OOM tokens");

    // have to re-init
    jsmn_init(&p);
    // build tokens
    int r = jsmn_parse(&p, json_buf, json_len, toks, (u32)n_tokens);
    CHECK_WARN_RET(r < 0, -1, "jsmn second pass failed");

    // this has to be true, otherwise, invalid json
    if (toks[0].type != JSMN_OBJECT) {
        free(toks);
        WARN("JSON root is not an object");
        return -1;
    }

    *tokens = toks;
    return n_tokens;
}

static bool build_json_tree(char* raw_json, jsmntok_t* tokens, wcjson* json)
{

}


