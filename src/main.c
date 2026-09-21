#include "common.h"
#include "views.h"
#include <string.h>


int main(void)
{
    StringStore ss;
    StringStore_create(&ss);

    StrView sv1 = StringStore_cstr(&ss, "hellow", strlen("hellow"));
    StrView sv2 = StringStore_cstr(&ss, "hellow", strlen("hellow"));
    StrView sv3 = StringStore_cstr(&ss, "hellow", strlen("hellow"));
    StrView sv4 = StringStore_cstr(&ss, "hellow", strlen("hellow"));
    StrView sv5 = StringStore_cstr(&ss, "hellow", strlen("hellow"));
    StrView sv6 = StringStore_cstr(&ss, "hellow", strlen("hellow"));
    StrView sv7 = StringStore_cstr(&ss, "hellow", strlen("hellow"));

    StrView_print(sv1);
    StrView_print(sv2);
    StrView_print(sv3);
    StrView_print(sv4);
    StrView_print(sv5);
    StrView_print(sv6);
    StrView_print(sv7);

    print_hex((const u8*)ss.head->buf, 124, 32);

    StringStore_destroy(&ss);
    return 0;
}
