#ifndef _SVM_H
#define _SVM_H

typedef struct _Node Node;

struct _Node {
    int val;
#ifdef __cplusplus
    struct _Node* next;
#else
    __global struct _Node* next;
#endif
};

#endif
