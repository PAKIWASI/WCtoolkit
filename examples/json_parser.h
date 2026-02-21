#ifndef WC_JSON_PARSER_H
#define WC_JSON_PARSER_H

#include "String.h"
#include "arena.h"



typedef enum {
    J_STR,
    J_NUM,
    J_BOOL,
    J_NULL,
    J_OBJ,
    J_ARR,
} json_type;

// Forward declaration
struct json_pair;

typedef struct {
    json_type type;

    union {
        String* str_val;   // J_STR  — heap-allocated String
        double  num_val;   // J_NUM
        b8      bool_val;  // J_BOOL

        struct {           // J_OBJ
            struct json_pair* pairs;
            u32               count;
        } obj_val;

        struct {           // J_ARR
            struct json_val** items;
            u32               count;
            u32               cap;
        } arr_val;
    };
} json_val;

// Key-value pair inside a JSON object
typedef struct json_pair {
    String   key;     // inline String (string_create_stk)
    json_val val;     // inline value (all types fit in the union)
} json_pair;


// CREATION / DESTRUCTION

// Allocate a json_val of the given type from the arena.
// J_STR: str_val is heap-allocated (must be freed via json_val_destroy).
// J_OBJ: pairs array is malloc'd (freed via json_val_destroy).
// J_ARR: items array is malloc'd (freed via json_val_destroy).
json_val* json_val_create(Arena* arena, json_type type);

// Recursively free any heap resources owned by val.
// Does NOT free val itself (it lives in the arena).
// Call this before releasing the arena if you need early cleanup.
void json_val_destroy(json_val* val);

// Allocate the initial pair array for an object node.
// Returns a malloc'd array of 8 json_pair slots.
json_pair* json_pair_create(Arena* arena);


// MAIN PARSE API  (prefer these over the individual readers)

/*
 * Parse raw JSON text into a json_val tree.
 *
 * All json_val nodes are arena-allocated.  Strings (J_STR keys and values),
 * pair arrays (J_OBJ), and item arrays (J_ARR) are heap-allocated and owned
 * by the tree — call json_val_destroy(root) before releasing the arena to
 * free them, or simply release the arena and accept the leak if the process
 * is short-lived.
 *
 * Returns the root json_val* on success, NULL on parse error.
 */
json_val* json_parse(const String* raw_text, Arena* arena);

/*
 * Load filename, parse it, return root.  Convenience wrapper around
 * load_json_file + json_parse.  Destroys the raw String after parsing.
 */
json_val* json_parse_file(const char* filename, Arena* arena);


// PATH-BASED LOOKUP

/*
 * Walk the json_val tree following a dot-separated key path.
 *
 *   get_value(root, path("user.address.city"))
 *   get_value(root, path("items.0.price"))   // array index as integer string
 *
 * Returns a pointer into the tree (not a copy) — valid as long as the tree
 * lives.  Returns NULL and emits a WARN if any segment is not found.
 */
json_val* get_value(json_val* root, const String* path);



// Pretty-print a json_val tree to stdout.
void print_value(const json_val* value);


// // LOW-LEVEL READERS  (exposed for completeness; prefer json_parse above)
//
// // Load a file into a heap-allocated String. Caller must string_destroy.
// String* load_json_file(const char* filename);
//
// // Parse a JSON string literal from the start of raw_text.
// // Returns a heap-allocated String. Caller must string_destroy.
// String* read_string(const String* raw_text);
//
// // Parse a JSON number from the start of raw_text.
// double read_number(const String* raw_text);
//
// // Parse "true" or "false" from the start of raw_text.
// b8 read_bool(const String* raw_text);
//
// // Parse a JSON object from raw_text (allocates its own arena internally).
// // Prefer json_parse() so you control the arena lifetime.
// json_val* read_object(const String* raw_text);
//
// // Parse any JSON value from raw_text (allocates its own arena internally).
// // Prefer json_parse() so you control the arena lifetime.
// json_val* read_value(const String* raw_text);
//
// // No-op stub (cursor is internal to the parser).
// void skip_spaces(const String* raw_text);


#endif // WC_JSON_PARSER_H
