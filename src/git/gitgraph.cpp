#include <gitgraph.h>
#include <stack>
#include <map>

static std::string shrink_commit_message(std::string msg){
    int i = -1;
    for(uint j = 0; j < msg.size(); j++){
        if(msg[j] == '\n' && j > 0){
            i = j;
            break;
        }
    }

    return msg.substr(0, i);
}

static std::string GitGraph_GetCommitHash(git_commit *cmt, uint n=9){
    const git_oid *oid = git_commit_id(cmt);
    std::string buf(git_oid_tostr_s(oid), n);
    return buf;
}

static GitGraphNode *GitGraph_FindNode(git_commit *commit,
                                       std::map<std::string, GitGraphNode *> *map,
                                       bool alloc=false)
{
    const git_oid *oid = git_commit_id(commit);
    std::string buf(git_oid_tostr_s(oid), 9);
    if(map->find(buf) == map->end()){
        if(!alloc) return nullptr;
        GitGraphNode *g = new GitGraphNode;
        g->commit = nullptr;
        return g;
    }
    return map->operator[](buf);
}

int cAlloc = 0;
int cDealloc = 0;
int gAlloc = 0;
int gDealloc = 0;

template<typename Fn>
uint GitGraph_DFSBuild(GitGraph *graph, const Fn &fn){
    struct info_v{
        git_commit *cmt;
        int it;
    };
    uint nodeCount = 0;
    GitGraphNode *gnode = graph->graph;

    std::stack<info_v> stack;
    std::vector<git_commit *> to_free;
    std::map<std::string, GitGraphNode *> cmtMap;

    stack.push({.cmt = gnode->commit, .it = 0});
    gnode->explored = false;
    cmtMap[GitGraph_GetCommitHash(gnode->commit)] = gnode;

    while(stack.size() > 0){
        info_v info = stack.top();
        stack.pop();

        git_commit *cmt = info.cmt;
        GitGraphNode *node = GitGraph_FindNode(cmt, &cmtMap);

        if(!node || !node->explored){
            if(!node){
                node = new GitGraphNode;
                graph->nodes.push_back(node);
                gAlloc += 1;
            }

            node->commit = cmt;
            node->i = info.it;
            node->explored = true;
            std::string buf = GitGraph_GetCommitHash(cmt);
            uint count = git_commit_parentcount(cmt);
            nodeCount += 1;

            fn(node->commit);

            if(count > 0){
                for(int i = count-1; i >= 0; i--){
                    git_commit *parent = nullptr;
                    git_commit_parent(&parent, cmt, i);
                    to_free.push_back(parent);
                    cAlloc += 1;

                    GitGraphNode *gparent = GitGraph_FindNode(parent, &cmtMap, true);
                    if(!gparent->commit){
                        git_commit_dup(&gparent->commit, parent);
                        gparent->explored = false;
                        std::string buf2 = GitGraph_GetCommitHash(parent);
                        cmtMap[buf2] = gparent;
                        graph->nodes.push_back(gparent);
                        gAlloc += 1;
                        cAlloc += 1;
                    }

                    gparent->childs.push_back(node);
                    node->parents.push_back(gparent);

                    stack.push({.cmt = parent, .it = info.it+1});
                }
            }
            cmtMap[buf] = node;
        }else{
            // found a new commit that already was handled
            // this is mostly because commits can show multiple
            // times if they refer to multiple branches
            // TODO: How to convert commit -> reference?
        }
    }

    for(git_commit *f : to_free){
        git_commit_free(f);
        cDealloc += 1;
    }
    return nodeCount;
}

static void get_str(git_commit *cmt, std::string &buf, std::string &msg){
    const git_oid *oid = git_commit_id(cmt);
    buf = std::string(git_oid_tostr_s(oid), 9);
    msg = shrink_commit_message(git_commit_message(cmt));
}

void GitGraph_Free(GitGraph *graph){
    for(GitGraphNode *node : graph->nodes){
        if(node->commit){
            git_commit_free(node->commit);
            cDealloc += 1;
        }
        delete node;
        gDealloc += 1;
    }

    printf("cAlloc = %d, cDealloc = %d, gAlloc = %d, gDealloc = %d\n",
            cAlloc, cDealloc, gAlloc, gDealloc);

    delete graph;
}

void GitGraph_Print(GitGraphNode *graph){
    std::vector<GitGraphNode *> depends;
    std::vector<GitGraphNode *> next;
    depends.push_back(graph);
    while(depends.size() > 0){
        next.clear();
        printf("[%lu] ", depends.size());
        for(GitGraphNode *g : depends){
            std::string buf, msg;
            get_str(g->commit, buf, msg);
            printf("(%s - %s) ", buf.c_str(), msg.c_str());
            for(GitGraphNode *b : g->parents){
                next.push_back(b);
            }
        }
        printf("\n");
        depends.clear();

        depends = next;
    }
}

GitGraph *GitGraph_Build(git_commit *commit){
    GitGraphNode *gnode = new GitGraphNode;
    GitGraph *ggraph = new GitGraph;
    gnode->commit = commit;
    ggraph->graph = gnode;

    uint n = GitGraph_DFSBuild(ggraph, [&](git_commit *cmt) -> void{
        const git_oid *oid = git_commit_id(cmt);
        std::string buf(git_oid_tostr_s(oid), 9);
        std::string msg = shrink_commit_message(git_commit_message(cmt));
        printf("%s - %s\n", buf.c_str(), msg.c_str());
    });

    printf("Graph with %d nodes\n", (int)n);
    GitGraph_Print(gnode);
    ggraph->graph = gnode;

    return ggraph;
}
