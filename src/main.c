#include "common.h"
#include "hashmap.h"
#include "wc_helpers.h"
#include "wc_macros.h"
#include "wc_string.h"





int main(void)
{
    HashMap* map = MAP_CREATE_OF(String, int);

    MAP_PUT_STR_INT(map, "hefjs", 5);
    MAP_PUT_STR_INT(map, "sjlkfjdkl", 5);
    MAP_PUT_STR_INT(map, "s", 5);
    MAP_PUT_STR_INT(map, "js", 5);

    HashMap_print(map, str_print, wc_print_int);


    HashMap_destroy(map);
    return 0;
}
