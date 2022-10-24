/* date = December 6th 2021 19:11 */
#pragma once
#include <git2.h>
#include <vector>
#include <string>
#include <gitbase.h>

typedef struct GitGraphNode{
    git_commit *commit;
    std::vector<std::string> refs;
    std::vector<GitGraphNode *> childs;
    std::vector<GitGraphNode *> parents;
    bool explored, is_merge_child;
    int i, j;
}GitGraphNode;

typedef struct GitGraph{
    GitGraphNode *graph;
    std::vector<GitGraphNode *> nodes;
}GitGraph;

/*
* Builds a graph based on the tree reachable from the given commit.
*/
GitGraph *GitGraph_Build(GitState *state);

/*
* Releases the memory from a given graph.
*/
void GitGraph_Free(GitGraph *graph);

