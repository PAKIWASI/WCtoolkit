#include "views.h"
#include <string.h>


int main(void)
{
    string_store ss;
    string_store_create(&ss);

    const char* c1 = "hello";
    const char* c2 = "world";
    strview sv1 = string_store_cstr(&ss, c1, strlen(c1));
    strview sv2 = string_store_cstr(&ss, c2, strlen(c2));
    strview_print(sv1);
    strview_print(sv2);

    string_store_destroy(&ss);
}
