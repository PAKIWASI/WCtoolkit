#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H


/* Priority Queue

    We implement it as a binary heap (a binary tree satisfing the heap property)
    Every parent has higher priority than its children (heap property)
    It is a complete tree: filled left to right, level by level
    Priority is based on a comparison function comparing 2 elements in the heap
    It is not sorted, it only garentees parents are better than the children
    We use vector as the container

          1
        /   \
       3     5
      / \   /
     9   4 6
    index:  0  1  2  3  4  5
    value: [1, 3, 5, 9, 4, 6]

    for index i:
        parent      = (i - 1) / 2;
        left child  = 2 * i + 1;
        right child = 2 * i + 2;


    Main Operations:

    1. peek() — get highest priority
    Just return items[0].
    O(1)

    2. push() — insert
        Put the new item at the end of the array.
        Compare it with its parent.
        If it has higher priority, swap them.
        Keep moving it up until the heap property is restored.
    This is called sift up or bubble up.
    O(log n)

    3. pop() — remove highest priority
        Save items[0] — that’s the answer.
        Move the last item into items[0].
        Shrink the size by one.
        Compare the new root with its children.
        Swap it with the highest-priority child.
        Keep moving it down until the heap property is restored.
    This is called sift down or bubble down.
    O(log n)

    4. heapify_down/up() - maintains the heap property
    O(n)

    5. build_heap() - create a new heap from a vector or array

*/

#endif // PRIORITY_QUEUE_H
