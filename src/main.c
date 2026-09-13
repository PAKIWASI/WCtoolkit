#include "views.h"
#include <string.h>


int main(void)
{
    StringStore ss;
    StringStore_create(&ss);

    const char* c1 = "hello";
    const char* c2 = "world";
    StrView sv1 = StringStore_cstr(&ss, c1, strlen(c1));
    StrView sv2 = StringStore_cstr(&ss, c2, strlen(c2));
    StrView_print(sv1);
    StrView_print(sv2);

    StringStore_destroy(&ss);
}
