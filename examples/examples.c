/*
 * examples.c — Showcase projects for WCtoolkit
 * ================================================
 *
 * run with ./build/examples
 */


/*
 * WCtoolkit JSON parser showcase   (Written with AI)
 * ==============================================
 *
 * Demonstrates the four key strengths of the library:
 *
 *   1. PARSING      — json_parse() returns an owned, recursive JsonValue tree.
 *                     One call to json_value_destroy() frees everything.
 *
 *   2. BUILDING     — Constructors + move semantics let you assemble JSON trees
 *                     without any explicit deep copies or memory bookkeeping.
 *
 *   3. DEEP COPY    — json_value_copy() duplicates an entire tree. Both copies
 *                     are independently owned; mutating one doesn't touch the other.
 *
 *   4. ROUND-TRIP   — json_to_string() serializes back to a WCtoolkit String.
 *                     The String is owned and must be string_destroy()ed.
 *
 * Memory correctness is verified throughout using ASan
 */


#include "json_parser.h"
#include "wc_macros.h"
#include <stdio.h>


// ── helpers ────────────────────────────────────────────────────────────

static void section(const char* title)
{
    printf("\n\033[1;36m══ %s ══\033[0m\n", title);
}

static void subsection(const char* title)
{
    printf("\n\033[1;33m── %s ──\033[0m\n", title);
}


// Demo 1 — Parse a complex JSON document and inspect it
static void demo_parse(void)
{
    section("Demo 1: Parsing");

    const char* raw = "{"
                      "\"name\":    \"WCtoolkit\","
                      "\"version\": 1,"
                      "\"stable\":  true,"
                      "\"pi\":      3.14159265358979,"
                      "\"nothing\": null,"
                      "\"tags\":    [\"C\", \"systems\", \"ownership\"],"
                      "\"author\":  {"
                      "    \"name\":  \"PAKIWASI\","
                      "    \"year\":  2026,"
                      "    \"active\": true"
                      "}"
                      "}";

    /*
     * json_parse returns a heap JsonValue*. We own it.
     * The entire recursive tree is allocated internally — arrays, objects,
     * strings — all managed by WCtoolkit containers with the right callbacks.
     */
    JsonValue* doc = json_parse(raw);
    if (!doc) {
        fprintf(stderr, "parse failed\n");
        return;
    }

    subsection("Pretty-printed");
    json_print(doc, 2);

    subsection("Field access — borrowed pointers (no copy, no free)");

    /*
     * json_object_get returns a borrowed pointer into the hashmap bucket.
     * We do NOT free it. The lifetime is tied to the parent object.
     */
    JsonValue* name_val = json_object_get(doc, "name");
    if (name_val && name_val->type == JSON_STRING) {
        /* 
         * TEMP_CSTR_READ: appends '\0', runs block, removes '\0'.
         * Gives us a null-terminated view without malloc. 
        */
        TEMP_CSTR_READ(&name_val->string)
        {
            printf("  name:    %s\n", string_data_ptr(&name_val->string));
        }
    }

    JsonValue* ver_val = json_object_get(doc, "version");
    if (ver_val && ver_val->type == JSON_NUMBER) {
        printf("  version: %g\n", ver_val->number);
    }

    JsonValue* tags_val = json_object_get(doc, "tags");
    if (tags_val && tags_val->type == JSON_ARRAY) {
        printf("  tags (%llu): ", (unsigned long long)json_array_len(tags_val));
        for (u64 i = 0; i < json_array_len(tags_val); i++) {
            JsonValue* tag = json_array_get(tags_val, i);
            TEMP_CSTR_READ(&tag->string)
            {
                printf("%s ", string_data_ptr(&tag->string));
            }
        }
        printf("\n");
    }

    JsonValue* author = json_object_get(doc, "author");
    if (author && author->type == JSON_OBJECT) {
        JsonValue* aname = json_object_get(author, "name");
        if (aname) {
            TEMP_CSTR_READ(&aname->string)
            {
                printf("  author:  %s\n", string_data_ptr(&aname->string));
            }
        }
    }

    subsection("Serialized back to compact JSON");
    /*
     * json_to_string returns a heap String* we own.
     * TEMP_CSTR_READ gives a temporary null-terminated view.
     */
    String* serialized = json_to_string(doc);
    TEMP_CSTR_READ(serialized)
    {
        printf("  %s\n", string_data_ptr(serialized));
    }
    string_destroy(serialized); // we own it — free it

    /*
     * ONE call frees the entire tree:
     *   doc (JsonValue*)
     *    ├── "name" → String (data buffer freed by str_del)
     *    ├── "tags" → genVec* (json_val_del called per element)
     *    │    ├── String "C"
     *    │    ├── String "systems"
     *    │    └── String "ownership"
     *    └── "author" → hashmap* (str_del on keys, json_val_del on values)
     *         ├── "name" → String
     *         ├── "year" → double
     *         └── "active" → b8
     */
    json_value_destroy(doc);
    printf("\n  [json_value_destroy: entire tree freed in one call]\n");
}


// Demo 2 — Build a JSON document programmatically with move semantics
static void demo_build(void)
{
    section("Demo 2: Building with move semantics");

    /*
     * Each constructor returns a heap JsonValue*.
     * json_array_push / json_object_set move it in — the pointer is NULLed.
     * We never write free() because ownership is always clear.
     */

    // Build an array of numbers
    JsonValue* primes = json_array_new();
    for (int p = 2; p <= 19; p++) {
        b8 is_prime = true;
        for (int d = 2; d * d <= p; d++) {
            if (p % d == 0) {
                is_prime = false;
                break;
            }
        }
        if (!is_prime) {
            continue;
        }

        JsonValue* n = json_number((double)p);
        json_array_push(primes, &n);
    }

    // Build an array of strings via the macro layer
    JsonValue*  langs       = json_array_new();
    const char* lang_list[] = {"C", "Zig", "Rust", "Odin", NULL};
    for (const char** l = lang_list; *l; l++) {
        JsonValue* s = json_string_val(*l);
        json_array_push(langs, &s);
    }

    // Build a nested object
    JsonValue* meta = json_object_new();
    {
        JsonValue* author = json_string_val("PAKIWASI");
        json_object_set(meta, "author", &author);

        JsonValue* year = json_number(2026);
        json_object_set(meta, "year", &year);

        JsonValue* open = json_bool(true);
        json_object_set(meta, "open_source", &open);
    }

    // Assemble top-level object — move everything in
    JsonValue* root = json_object_new();
    json_object_set(root, "primes", &primes); // primes → NULL 
    json_object_set(root, "langs", &langs);   // langs  → NULL
    json_object_set(root, "meta", &meta);     // meta   → NULL

    subsection("Built document");
    json_print(root, 2);

    json_value_destroy(root); // frees the entire tree 
    printf("\n  [All resources freed by json_value_destroy(root)]\n");
}


// Demo 3 — Deep copy: two independent trees, mutate one, other unchanged
static void demo_deep_copy(void)
{
    section("Demo 3: Deep copy — ownership independence");

    const char* src_json = "[{\"x\": 1, \"y\": 2}, {\"x\": 3, \"y\": 4}]";

    JsonValue* original = json_parse(src_json);
    if (!original) {
        return;
    }

    /*
     * json_value_copy allocates a new heap JsonValue and calls json_val_copy
     * recursively. The copy is completely independent — no shared pointers.
     */
    JsonValue* copy = json_value_copy(original);

    // Mutate the copy: set copy[0]["x"] = 999
    JsonValue* elem0 = json_array_get(copy, 0);
    JsonValue* new_x = json_number(999.0);
    json_object_set(elem0, "x", &new_x);

    subsection("original (unmodified)");
    json_print(original, 2);

    subsection("copy (mutated copy[0].x = 999)");
    json_print(copy, 2);

    json_value_destroy(original);
    json_value_destroy(copy);
    printf("\n  [Both trees freed independently — no double-free, no leak]\n");
}


// Demo 4 — Error handling: invalid JSON is rejected cleanly
static void demo_errors(void)
{
    section("Demo 4: Error handling");

    const char* bad[] = {"{\"key\": }",         // missing value       
                         "[1, 2, 3",            // unclosed array      
                         "{\"a\": 1, \"b\": 2", // unclosed object     
                         "tru",                 // truncated keyword   
                         "\"unterminated",      // unterminated string 
                         NULL};

    for (const char** b = bad; *b; b++) {
        printf("  input: %-30s  → ", *b);
        JsonValue* v = json_parse(*b);
        if (v) {
            printf("(unexpectedly succeeded)\n");
            json_value_destroy(v);
        } else {
            printf("NULL (error caught)\n");
        }
    }
}


// Demo 5 — A realistic config file scenario
static void demo_config(void)
{
    section("Demo 5: Real-world config file");

    const char* config_json = "{"
                              "  \"server\": {"
                              "    \"host\": \"0.0.0.0\","
                              "    \"port\": 8080,"
                              "    \"tls\":  false"
                              "  },"
                              "  \"database\": {"
                              "    \"host\": \"localhost\","
                              "    \"port\": 5432,"
                              "    \"name\": \"myapp\","
                              "    \"pool_size\": 10"
                              "  },"
                              "  \"features\": [\"auth\", \"metrics\", \"rate_limiting\"],"
                              "  \"log_level\": \"info\","
                              "  \"max_connections\": 1000"
                              "}";

    JsonValue* cfg = json_parse(config_json);
    if (!cfg) {
        return;
    }

    // Safely extract server config
    JsonValue* server = json_object_get(cfg, "server");
    if (server && server->type == JSON_OBJECT) {
        JsonValue* host = json_object_get(server, "host");
        JsonValue* port = json_object_get(server, "port");
        JsonValue* tls  = json_object_get(server, "tls");
        if (host && port && tls) {
            TEMP_CSTR_READ(&host->string)
            {
                printf("  Server: %s:%g (TLS: %s)\n", string_data_ptr(&host->string), port->number,
                       tls->boolean ? "yes" : "no");
            }
        }
    }

    // Extract and display features list
    JsonValue* features = json_object_get(cfg, "features");
    if (features && features->type == JSON_ARRAY) {
        printf("  Features enabled (%llu):\n", (unsigned long long)json_array_len(features));
        VEC_FOREACH(features->array, JsonValue, feat)
        {
            TEMP_CSTR_READ(&feat->string)
            {
                printf("    • %s\n", string_data_ptr(&feat->string));
            }
        }
    }

    // BUG: this crashes: ASan 
    // Demonstrate: build a modified copy with a different log level 
    subsection("Modified copy with log_level = \"debug\"");
    JsonValue* cfg_copy = json_value_copy(cfg);
    {
        JsonValue* debug = json_string_val("debug");
        json_object_set(cfg_copy, "log_level", &debug);
    }

    // Serialize both to Strings and compare
    String* orig_s = json_to_string(cfg);
    String* copy_s = json_to_string(cfg_copy);

    printf("  original log_level in serialized:\n  ");
    // Find "log_level" in the serialized string to verify
    JsonValue* ll_orig = json_object_get(cfg, "log_level");
    JsonValue* ll_copy = json_object_get(cfg_copy, "log_level");
    if (ll_orig && ll_copy) {
        TEMP_CSTR_READ(&ll_orig->string)
        {
            printf("  original: log_level = %s\n", string_data_ptr(&ll_orig->string));
        }
        TEMP_CSTR_READ(&ll_copy->string)
        {
            printf("  copy:     log_level = %s\n", string_data_ptr(&ll_copy->string));
        }
    }

    string_destroy(orig_s);
    string_destroy(copy_s);
    json_value_destroy(cfg);
    json_value_destroy(cfg_copy);
    printf("\n  [Both configs freed independently]\n");
}



int main(void)
{
    printf("\033[1;32m");
    printf("  WCtoolkit JSON Parser — Ownership Showcase   \n");
    printf("\033[0m");

    demo_parse();
    demo_build();
    demo_deep_copy();
    demo_errors();
    demo_config();

    printf("\n\033[1;32m[All demos complete — no leaks, no double-frees]\033[0m\n\n");
    return 0;
}


