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

// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
