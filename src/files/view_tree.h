#pragma once
#include <view.h>
#include <bufferview.h>
#include <functional>
#include <stack>

// The geometry in each node does not contain pixels information
// it only holds the reference percentage of screen taken
typedef struct ViewNode{
    View *view;
    int is_leaf;
    Geometry geometry;
    ViewNode *child0, *child1;
    ViewNode *parent;
}ViewNode;

typedef struct ViewTreeIterator{
    std::stack<ViewNode *> stack;
    ViewNode *value;
}ViewTreeIterator;

typedef struct ViewTree{
    ViewNode *root;
    ViewNode *activeNode;
}ViewTree;

/*
* Initializes the view tree
*/
void ViewTree_Initialize();

/*
* Get the active current node of the view tree.
*/
ViewNode *ViewTree_GetCurrent();

/*
* Updates the active tree node to be vnode. You should get the node vnode from
* a query either using the Begin/Next routines or the ForAll routine.
*/
void ViewTree_SetActive(ViewNode *vnode);

/*
* Begins a iteration in the view tree, returns only leafs
*/
void ViewTree_Begin(ViewTreeIterator *iterator);

/*
* Performs one step in a view tree iteration to the next leaf
*/
void ViewTree_Next(ViewTreeIterator *iterator);

/*
* Splits the geometry contents of the active view tree node
* horizontaly, i.e.: on x-direction.
*/
void ViewTree_SplitHorizontal();

/*
* Splits the geometry contents of the active view tree node
* verticaly, i.e.: on y-direction.
*/
void ViewTree_SplitVertical();

/*
* Destroy the current active view tree node merging views if required, returns
* the current active node so that UI can set target properties
*/
ViewNode *ViewTree_DestroyCurrent(int free_view=0);

/*
* Execute function on all leaf views
*/
void ViewTree_ForAllViews(std::function<int(ViewNode *)> fn);