#include "common.h"
#include "gen_vector.h"
#include "hashmap.h"
#include "wc_macros.h"
#include <stdio.h>


/*
Given an array of integers nums and an integer target, return indices of the two numbers such that they add up to target.
You may assume that each input would have exactly one solution, and you may not use the same element twice.
You can return the answer in any order.

Input: nums = [2,7,11,15], target = 9
Output: [0,1]
Explanation: Because nums[0] + nums[1] == 9, we return [0, 1].
*/
static genVec* two_sum(const int* arr, u32 size, int target)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(u32),
                                  NULL, NULL, NULL, NULL );
    genVec* v = VEC(u32, 2);

    for (u32 i = 0; i < size; i++) {
        int remaining = target - arr[i];
        u32 idx = -1;
        b8 has = hashmap_get(map, cast(remaining), cast(idx));
        printf("%b, %u\n", has, idx);
        if (has) {
            int a = 5;
            genVec_push(v, cast(a));
            break;
        }
        MAP_PUT(map, cast(arr[i]), cast(i));
    }

    hashmap_destroy(map);
    return v;
}




int main(void)
{
    int arr1[4] = {2, 7, 11, 15};
    genVec* res = two_sum(arr1, 4, 9);
    genVec_print(res, wc_print_u32);


    genVec_destroy(res);
    return 0;
}
