#ifndef _SPACEX_MERKLE_TREE_H_
#define _SPACEX_MERKLE_TREE_H_

#include <stddef.h>

typedef struct MerkleTreeStruct
{
    char* hash;
    size_t size;
    size_t links_num;
    struct MerkleTreeStruct** links;
} MerkleTree;

#endif /* !_SPACEX_MERKLE_TREE_H_ */
