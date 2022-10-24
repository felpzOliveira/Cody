#include <gitgraph.h>
#include <gitbase.h>
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
    gnode->is_merge_child = false;
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
                        gparent->is_merge_child = i > 0;
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
            if(g->is_merge_child){
                printf("[%d](%s - %s)* ", g->i, buf.c_str(), msg.c_str());
            }else{
                printf("[%d](%s - %s) ", g->i, buf.c_str(), msg.c_str());
            }
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
        //const git_oid *oid = git_commit_id(cmt);
        //std::string buf(git_oid_tostr_s(oid), 9);
        //std::string msg = shrink_commit_message(git_commit_message(cmt));
        //printf("%s - %s\n", buf.c_str(), msg.c_str());
    });

    printf("Graph with %d nodes\n", (int)n);
    GitGraph_Print(gnode);
    ggraph->graph = gnode;

    return ggraph;
}

PriorityQueue<commit_info> *Git_GetStartingCommits(GitState *state, bool local_only){
    PriorityQueue<commit_info> *pq = nullptr;
    if(state->repo){
        std::map<std::string, commit_info *> cmtMap;
        pq = PriorityQueue_Create<commit_info>(commit_info_comp);
        auto iterator = [&](git_reference *ref, GitReferenceType type) -> bool{
            if(type == GIT_REF_BRANCH_LOCAL ||
               (!local_only && type == GIT_REF_BRANCH_REMOTE))
            {
                const char *branchName = nullptr;
                git_branch_name(&branchName, ref);

                // TODO: figure out how to add HEAD into viweing
                std::string name(branchName);
                if(name == "origin/HEAD" || name == "HEAD") return true;

                commit_info *info = new commit_info;

                git_reference_name_to_id(&info->oid, state->repo, git_reference_name(ref));
                git_commit_lookup(&info->commit, state->repo, &info->oid);

                std::string h1 = GitGraph_GetCommitHash(info->commit);
                if(cmtMap.find(h1) == cmtMap.end()){
                    info->type = type;
                    info->branch = branchName;
                    info->time = git_commit_time(info->commit);
                    PriorityQueue_Push(pq, info);
                    cmtMap[h1] = info;
                }else{
                    git_commit_free(info->commit);
                    delete info;
                }
            }
            return true;
        };

        Git_TransverseRelevantReferences(state, iterator);
    }
    return pq;
}

GitGraph *GitGraph_Build(GitState *state){
    PriorityQueue<commit_info> *pq = Git_GetStartingCommits(state, false);

    commit_info *info = PriorityQueue_Peek(pq);
    GitGraph *graph = GitGraph_Build(info->commit);
    GitGraph_Free(graph);

    PriorityQueue_Free(pq);
    return nullptr;
}
