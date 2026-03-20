#include "String.h"
#include "hashset.h"
#include "wc_helpers.h"




int main(void)
{
    hashset* set = hashset_create(sizeof(String), wyhash_str, str_cmp, &wc_str_ops);

    String* s = string_from_cstr("hello");
    u64 s_mark = string_len(s);
    char buff[3] = {0};
    buff[0] = '0';
    buff[2] = '\0';
    for (int i = 0; i < 100; i++) {

        int j = i;
        int k = 0;
        while (j > 0) {
            buff[k] = (char)('1' + (j % 10));
            j /= 10;
            k++;
        }

        string_append_cstr(s, buff);

        hashset_insert(set, (u8*)s);
        
        string_remove_range(s, s_mark, string_capacity(s));
    }

    hashset_print(set, str_print);


    string_destroy(s);
    hashset_destroy(set);
    return 0;
}

