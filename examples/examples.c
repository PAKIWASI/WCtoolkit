/*
 * examples.c — Example Programmes using WCtoolkit
 *
*/



#include "arena.h"
#include "common.h"
#include "json_parser.h"

// TODO: breaks on nested obj?

int main(void)
{
    Arena* arena = arena_create(nKB(1));
    json_val* val = json_parse_file("examples/package.json", arena);

    print_value(val);   

    json_val_destroy(val);
    arena_release(arena);
    return 0;
}

