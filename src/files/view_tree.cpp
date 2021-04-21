#include <view_tree.h>
#include <stack>
#include <view.h>

static ViewTree viewTree = {
    .root = nullptr,
    .activeNode = nullptr,
};

ViewNode *ViewTree_CreateNode(int is_leaf=1){
    ViewNode *node = AllocatorGetN(ViewNode, 1);
    node->view = nullptr;
    node->is_leaf = is_leaf;
    node->child0 = nullptr;
    node->child1 = nullptr;
    node->parent = nullptr;
    if(is_leaf){
        node->view = AllocatorGetN(View, 1);
        View_EarlyInitialize(node->view);
        View_SetActive(node->view, 0);
        if(viewTree.root){
            BufferView *bView = &node->view->bufferView;
            BufferView_Initialize(bView, nullptr);
        }
    }

    return node;
}

void ViewTree_Initialize(){
    if(viewTree.root == nullptr){
        Geometry geometry;
        viewTree.root = ViewTree_CreateNode();
        geometry.extensionX = vec2f(0, 1);
        geometry.extensionY = vec2f(0, 1);
        viewTree.root->geometry = geometry;
        viewTree.root->view->geometry.extensionY = geometry.extensionY;
        viewTree.root->view->geometry.extensionX = geometry.extensionX;
        viewTree.activeNode = viewTree.root;
    }
}

ViewNode *ViewTree_GetCurrent(){
    if(viewTree.root){
        return viewTree.activeNode;
    }

    return nullptr;
}

static void ViewTree_IteratorNext(ViewTreeIterator *iterator){
    // DFS
    while(iterator->stack.size() > 0){
        ViewNode *aux = iterator->stack.top();
        iterator->stack.pop();

        if(aux->is_leaf == 1){
            iterator->value = aux;
            return;
        }else{
            AssertA(aux->child0 && aux->child1, "Invalid view tree node");
            // Push right child first so pop gets left first
            if(aux->child1){
                iterator->stack.push(aux->child1);
            }

            if(aux->child0){
                iterator->stack.push(aux->child0);
            }
        }
    }

    iterator->value = nullptr;
}

void ViewTree_Begin(ViewTreeIterator *iterator){
    if(viewTree.root){
        iterator->stack = std::stack<ViewNode *>(); // clear stack
        iterator->stack.push(viewTree.root);
        return ViewTree_IteratorNext(iterator);
    }
    iterator->value = nullptr;
}

void ViewTree_Next(ViewTreeIterator *iterator){
    if(viewTree.root){
        return ViewTree_IteratorNext(iterator);
    }

    iterator->value = nullptr;
}

void ViewTree_SetActive(ViewNode *vnode){
    viewTree.activeNode = vnode;
}

void ViewTree_ForAllViews(std::function<int(ViewNode *)> fn){
    ViewNode *root = viewTree.root;
    std::stack<ViewNode *> stack;
    //DFS
    stack.push(root);
    while(stack.size() > 0){
        ViewNode *node = stack.top();
        stack.pop();

        if(node->is_leaf){
            if(fn(node)){
                break;
            }
        }

        // Push right child first so pop gets left first
        if(node->child1){
            stack.push(node->child1);
        }

        if(node->child0){
            stack.push(node->child0);
        }
    }
}

ViewNode *ViewTree_DestroyCurrent(int free_view){
    if(viewTree.activeNode){
        ViewNode *active = viewTree.activeNode;
        if(active == viewTree.root){
            viewTree.root->view = nullptr;
            viewTree.root->is_leaf = 1;
            viewTree.root->child0 = nullptr;
            viewTree.root->child1 = nullptr;
            viewTree.root->parent = nullptr;
            viewTree.activeNode = viewTree.root;
        }else{
            ViewNode *parent = active->parent;
            AssertA(parent != nullptr, "Invalid parent configuration");
            ViewNode *otherChild = active == parent->child0 ? parent->child1 : parent->child0;
            parent->is_leaf = 1;
            parent->view = otherChild->view;
            if(free_view && active->view){
                View_Free(active->view);
            }
            AllocatorFree(parent->child0);
            AllocatorFree(parent->child1);

            parent->child0 = nullptr;
            parent->child1 = nullptr;
            parent->view->geometry.extensionY = parent->geometry.extensionY;
            parent->view->geometry.extensionX = parent->geometry.extensionX;
            viewTree.activeNode = parent;
        }

        return viewTree.activeNode;
    }

    return nullptr;
}

void ViewTree_SplitVertical(){
    if(viewTree.activeNode){
        Geometry smallGeometry, olderGeometry;
        ViewNode *active = viewTree.activeNode;
        ViewNode *smallChild = ViewTree_CreateNode(0);
        ViewNode *olderChild = ViewTree_CreateNode();
        Float dy = active->geometry.extensionY.y - active->geometry.extensionY.x;

        // For vertical split we put the left child at the top
        // so iteration follows up -> down logic
        smallGeometry.extensionY = vec2f(active->geometry.extensionY.x + dy * 0.5,
                                         active->geometry.extensionY.x + dy);

        olderGeometry.extensionY = vec2f(active->geometry.extensionY.x,
                                         active->geometry.extensionY.x + dy * 0.5);

        smallGeometry.extensionX = active->geometry.extensionX;
        olderGeometry.extensionX = active->geometry.extensionX;

        active->is_leaf = 0;
        smallChild->is_leaf = 1;
        smallChild->geometry = smallGeometry;
        smallChild->view = active->view;
        smallChild->parent = active;
        smallChild->view->geometry.extensionY = smallGeometry.extensionY;
        smallChild->view->geometry.extensionX = smallGeometry.extensionX;

        olderChild->geometry = olderGeometry;
        olderChild->is_leaf = 1;
        olderChild->parent = active;
        olderChild->view->geometry.extensionY = olderGeometry.extensionY;
        olderChild->view->geometry.extensionX = olderGeometry.extensionX;

        active->child0 = smallChild;
        active->child1 = olderChild;
        active->view = nullptr;
        viewTree.activeNode = smallChild;
    }
}

void ViewTree_SplitHorizontal(){
    if(viewTree.activeNode){
        Geometry smallGeometry, olderGeometry;
        ViewNode *active = viewTree.activeNode;
        ViewNode *smallChild = ViewTree_CreateNode(0);
        ViewNode *olderChild = ViewTree_CreateNode();
        Float dx = active->geometry.extensionX.y - active->geometry.extensionX.x;

        smallGeometry.extensionX = vec2f(active->geometry.extensionX.x,
                                         active->geometry.extensionX.x + dx * 0.5);

        olderGeometry.extensionX = vec2f(active->geometry.extensionX.x + dx * 0.5,
                                         active->geometry.extensionX.x + dx);

        smallGeometry.extensionY = active->geometry.extensionY;
        olderGeometry.extensionY = active->geometry.extensionY;

        active->is_leaf = 0;

        smallChild->is_leaf = 1;
        smallChild->view = active->view;
        smallChild->geometry = smallGeometry;
        smallChild->parent = active;
        smallChild->view->geometry.extensionY = smallGeometry.extensionY;
        smallChild->view->geometry.extensionX = smallGeometry.extensionX;

        olderChild->geometry = olderGeometry;
        olderChild->is_leaf = 1;
        olderChild->parent = active;
        olderChild->view->geometry.extensionY = olderGeometry.extensionY;
        olderChild->view->geometry.extensionX = olderGeometry.extensionX;

        active->child0 = smallChild;
        active->child1 = olderChild;
        active->view = nullptr;
        viewTree.activeNode = smallChild;
    }
}

