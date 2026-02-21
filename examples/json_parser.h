/*
 * json_parser.h — A JSON parser built on WCtoolkit
 * ============================================
 *
 * JSON type  →  WCtoolkit representation
 * ─────────────────────────────────────────────────────────────────
 * null       →  (tag only)
 * boolean    →  b8
 * number     →  double
 * string     →  String  (typedef genVec — dynamic char array)
 * array      →  genVec* (vec of JsonValue, by pointer)
 * object     →  hashmap* (String key → JsonValue value)
 *
 * OWNERSHIP RULES
 * ─────────────────────────────────────────────────────────────────
 * json_parse()             — returns heap JsonValue*. You own it.
 * json_value_destroy()     — frees heap JsonValue* and all resources.
 * json_value_destroy_stk() — frees resources inside a stack JsonValue.
 * json_value_copy()        — deep-copies any JsonValue tree.
 * json_array_push()        — moves *elem into arr; *elem → NULL.
 * json_object_set()        — moves *val into obj; *val → NULL.
 */

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "String.h"
#include "hashmap.h"


/*
 * JsonValue — tagged union
 *
 * WHY genVec* for arrays? The array genVec needs json_val_* callbacks
 * registered at creation time so recursive push/destroy works automatically.
 * Storing genVec* (8 bytes) in the union lets us attach those callbacks
 * once at json_array_new() and never touch them again. Same for hashmap*.
*/

typedef enum {
    JSON_NULL   = 0,
    JSON_BOOL   = 1,
    JSON_NUMBER = 2,
    JSON_STRING = 3,
    JSON_ARRAY  = 4,
    JSON_OBJECT = 5,
} JsonType;

typedef struct JsonValue JsonValue;


struct JsonValue {
    JsonType type;
    union {
        b8       boolean;
        double   number;
        String   string; // inline genVec — data buffer is heap-allocated
        genVec*  array;  // genVec<JsonValue> with json_val_* callbacks
        hashmap* object; // String key → JsonValue value
    };
};


/*
 * WCtoolkit callbacks — must be registered in every container that holds
 * JsonValues. With them in place, push/pop/copy/destroy of the container
 * automatically handles the full recursive tree.
*/
void json_val_copy(u8* dest, const u8* src);
void json_val_move(u8* dest, u8** src);
void json_val_del(u8* elm);
void json_val_print(const u8* elm);


// Lifecycle
void       json_value_destroy(JsonValue* val);
void       json_value_destroy_stk(JsonValue* val);
JsonValue* json_value_copy(const JsonValue* src);

// Constructors
JsonValue* json_null(void);
JsonValue* json_bool(b8 val);
JsonValue* json_number(double val);
JsonValue* json_string_val(const char* cstr);
JsonValue* json_array_new(void);
JsonValue* json_object_new(void);

// Array helpers
void       json_array_push(JsonValue* arr, JsonValue** elem); // moves elem in, *elem → NULL
u64        json_array_len(const JsonValue* arr);
JsonValue* json_array_get(const JsonValue* arr, u64 i); // borrow — do NOT free

// Object helpers
void       json_object_set(JsonValue* obj, const char* key, JsonValue** val); // moves val in
JsonValue* json_object_get(JsonValue* obj, const char* key);                  // borrow — do NOT free
b8         json_object_has(const JsonValue* obj, const char* key);

// Parser — returns heap JsonValue*, caller must json_value_destroy() it
JsonValue* json_parse(const char* input);

// Output
void    json_print(const JsonValue* val, int indent_width);
String* json_to_string(const JsonValue* val); // caller must string_destroy() result


#endif // JSON_PARSER_H
