#include <gitbase.h>
#include <git2.h>
#include <iostream>
#include <app.h>
#include <types.h>
#include <time.h>
#include <utilities.h>
#include <stack>
#include <gitgraph.h>

#define MODULE_NAME "GIT"
#define GIT_DISABLED_RET() do{ if(gitState.is_disabled) return; }while(0)

struct GitState{
    git_repository *repo;
    git_reference *head;
    git_diff *diff;
    bool is_disabled;
};

GitState gitState = {.repo=nullptr, .head=nullptr, .diff=nullptr, .is_disabled=false};

/*
* libgit2 has a bug on git_branch_next function regarding some pointers it handles.
* Compiling it with -O2/-Ofast makes that function generate a SIGSEGV so we have
* to bypass it and make a stable version for listing branches. After a little toying
* we see it is a simple git_reference_* manager, so we just do that manually here
* and hope the bug does not lie in the git_reference API. libgit2 has the bad principle
* to not initialize its variables so it might be that they are pointing to somewhere
* broken and missing a check. Since the issue only happens when debug is not enabled
* I can't be sure where the issue is without a heavy debug session, so I'll rewrite
* that function.
*/
/**********************************************************************/
#define GIT_REFS_DIR (char *)"refs/"
#define GIT_REFS_HEADS_DIR GIT_REFS_DIR "heads/"
#define GIT_REFS_TAGS_DIR GIT_REFS_DIR "tags/"
#define GIT_REFS_REMOTES_DIR GIT_REFS_DIR "remotes/"
#define GIT_REFS_NOTES_DIR GIT_REFS_DIR "notes/"

typedef enum{
    GIT_REF_NONE = 0,
    GIT_REF_TAG,
    GIT_REF_BRANCH_LOCAL,
    GIT_REF_BRANCH_REMOTE,
}GitReferenceType;

const char *GitReferenceString(GitReferenceType type){
    switch(type){
        case GIT_REF_TAG : return "Tag";
        case GIT_REF_BRANCH_LOCAL : return "Local Branch";
        case GIT_REF_BRANCH_REMOTE : return "Remote Branch";
        default: return "Unknow";
    }
}

// The function fn should return true in case it wants to keep transversing
// or false if it should interrupt
template<typename Fn> static bool Git_TransverseRelevantReferences(const Fn &fn){
    if(!gitState.repo) return false;
    int rv = 0, err = 0;
    git_reference_iterator *it = nullptr;
    git_reference *ref = nullptr;
    bool keep = true;

    rv = git_reference_iterator_new(&it, gitState.repo);
    if(rv != 0){
        DEBUG_MSG("Failed to create references iterator\n");
        return false;
    }

    while((err = git_reference_next(&ref, it)) == 0 && keep){
        GitReferenceType type = GIT_REF_NONE;
        const char *ptr = git_reference_name(ref);
        uint len = strlen(ptr);
        if(StringStartsWith((char *)ptr, len,
            GIT_REFS_TAGS_DIR, strlen(GIT_REFS_TAGS_DIR)))
        {
            type = GIT_REF_TAG;
        }else if(StringStartsWith((char *)ptr, len,
            GIT_REFS_HEADS_DIR, strlen(GIT_REFS_HEADS_DIR)))
        {
            type = GIT_REF_BRANCH_LOCAL;
        }else if(StringStartsWith((char *)ptr, len,
            GIT_REFS_REMOTES_DIR, strlen(GIT_REFS_REMOTES_DIR)))
        {
            type = GIT_REF_BRANCH_REMOTE;
        }

        if(type != GIT_REF_NONE){
            keep = fn(ref, type);
        }

        git_reference_free(ref);
        ref = nullptr;
    }

    git_reference_iterator_free(it);

    if(err != GIT_ITEROVER && err != 0){
        rv = -1;
    }

    return rv == 0;
}
/**********************************************************************/

static std::string Git_GetCommitHash(git_commit *cmt, uint n=9){
    const git_oid *oid = git_commit_id(cmt);
    std::string buf(git_oid_tostr_s(oid), n);
    return buf;
}

static bool Git_TryOpenRepo(git_repository **repo, git_reference **head, std::string root){
    int rv = 0;

    rv = git_repository_open(repo, root.c_str());
    if(rv != 0){
        DEBUG_MSG("Failed to open repository at [%s]\n", root.c_str());
        git_error_clear();
        return false;
    }else{
        //DEBUG_MSG("Opened repository at [%s]\n", root.c_str());
    }

    rv = git_repository_head(head, *repo);
    if(rv == GIT_EUNBORNBRANCH || rv == GIT_ENOTFOUND){
        // this is fine
        rv = 0;
    }else if(rv != 0){
        DEBUG_MSG("Failed to read repository head\n");
    }

    return rv == 0;
}

bool Git_OpenDirectory(char *path){
    if(gitState.is_disabled) return false;
    bool rv = false;
    git_repository *repo = nullptr;
    git_reference *head = nullptr;
    if(Git_TryOpenRepo(&repo, &head, std::string(path))){
        if(gitState.head) git_reference_free(gitState.head);
        if(gitState.repo) git_repository_free(gitState.repo);

        gitState.repo = repo;
        gitState.head = head;
        rv = true;
    }

    return rv;
}

bool Git_OpenRootRepository(){
    std::string root = AppGetRootDirectory();
    return Git_OpenDirectory((char *)root.c_str());
}

bool Git_GetReferenceHeadName(std::string &name){
    // check if the repo was successfully opened
    if(!gitState.head || gitState.is_disabled) return false;
    const char *refstr = git_reference_shorthand(gitState.head);
    if(refstr){
        name = std::string(refstr);
        return true;
    }else{
        DEBUG_MSG("Reference shorthand is nullptr\n");
    }

    return false;
}

bool Git_FetchStatus(std::vector<std::string> *_status){
    if(!gitState.repo) return false;
    git_status_list *status = nullptr;
    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX
                | GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;

    int rv = git_status_list_new(&status, gitState.repo, &opts);
    if(rv != 0){
        DEBUG_MSG("Failed to compute status\n");
        if(status) git_status_list_free(status);
        return false;
    }

    size_t n = git_status_list_entrycount(status);

    for(size_t i = 0; i < n; i++){
        const git_status_entry *entry = git_status_byindex(status, i);
        if(entry->status == GIT_STATUS_CURRENT) continue;

        std::string str;
        if((entry->status & GIT_STATUS_INDEX_NEW) ||
           (entry->status & GIT_STATUS_WT_NEW))
        {
            str = GIT_NEW_FILE_STR;
        }else if((entry->status & GIT_STATUS_INDEX_MODIFIED) ||
                 (entry->status & GIT_STATUS_WT_MODIFIED))
        {
            str = GIT_MODIFIED_FILE_STR;
        }else if((entry->status & GIT_STATUS_INDEX_DELETED) ||
                 (entry->status & GIT_STATUS_WT_DELETED))
        {
            str = GIT_DELETED_FILE_STR;
        }else if((entry->status & GIT_STATUS_INDEX_RENAMED) ||
                 (entry->status & GIT_STATUS_WT_RENAMED))
        {
            str = GIT_RENAMED_FILE_STR;
        }else if((entry->status & GIT_STATUS_INDEX_TYPECHANGE) ||
                 (entry->status & GIT_STATUS_WT_TYPECHANGE))
        {
            str = GIT_TYPECHANGE_FILE_STR;
        }

        if(str.size() == 0) continue;

        const char *old_path = nullptr;
        const char *new_path = nullptr;
        if(entry->head_to_index){
            old_path = entry->head_to_index->old_file.path;
            new_path = entry->head_to_index->new_file.path;
        }else{
            old_path = entry->index_to_workdir->old_file.path;
            new_path = entry->index_to_workdir->new_file.path;
        }

        std::string val(str);
        val += " ";
        if(old_path && new_path && strcmp(old_path, new_path)){
            val += std::string(old_path);
            val += " -> "; val += std::string(new_path);
        }else{
            val += std::string(old_path ? old_path : new_path);
        }

        _status->push_back(val);
    }


    git_status_list_free(status);
    return true;
}

bool Git_FetchDiffDeltas(std::vector<std::string> *_deltas){
    if(!gitState.repo || gitState.is_disabled) return false;
    if(!gitState.diff) return false;

    auto fn = [](const git_diff_delta *delta, float, void *obj) -> int{
        std::vector<std::string> *list = (std::vector<std::string> *)obj;
        if(list){
            if(!delta->new_file.path && delta->old_file.path){
                // file removed
                std::string val(GIT_DELETED_FILE_STR);
                val += delta->old_file.path;
                list->push_back(val);
            }else if(!delta->old_file.path && delta->new_file.path){
                std::string val(GIT_NEW_FILE_STR);
                val += delta->old_file.path;
                list->push_back(val);
            }else if(delta->old_file.path && delta->new_file.path){
                std::string path(delta->new_file.path);
                std::string val(GIT_MODIFIED_FILE_STR);
                if(path != delta->old_file.path){
                    val = GIT_RENAMED_FILE_STR;
                }
                val += path;
                list->push_back(val);
            }else{
                // don't know what is this?
            }
        }

        return 0;
    };

    int rv = git_diff_foreach(gitState.diff, fn, NULL, NULL, NULL, (void *)_deltas);
    if(rv != 0){
        DEBUG_MSG("Failed to compute diff\n");
    }

    return rv == 0;
}

bool Git_FetchDiffFor(const char *target, std::vector<LineHighlightInfo> *_hunks){
    if(!gitState.repo || gitState.is_disabled) return false;
    int rv = -1;
    git_diff *diff = nullptr;
    git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;

    diffopts.pathspec.strings = (char **)&target;
    diffopts.pathspec.count = 1;

    struct diff_data{
        std::vector<LineHighlightInfo> *hks;
        std::string tgt;
    };

    diff_data data = {.hks = _hunks, .tgt = std::string(target)};

    auto fn = [](const git_diff_delta *delta, const git_diff_hunk *hunk,
                 const git_diff_line *line, void * obj) -> int
    {
        diff_data *dt = (diff_data *)obj;
        if(!delta->new_file.path) return 0;

        std::string hpath(delta->new_file.path);
        if(hpath == dt->tgt){
            LineHighlightInfo hunk;
            std::vector<LineHighlightInfo> *hunks = dt->hks;

            hunk.content = std::string(line->content, line->content_len-1);
            if(line->origin == GIT_DIFF_LINE_DELETION){ // this line was removed
                hunk.ctype = GIT_LINE_REMOVED;
                hunk.lineno = line->old_lineno;
            }else if(line->origin == GIT_DIFF_LINE_ADDITION){
                hunk.ctype = GIT_LINE_INSERTED;
                hunk.lineno = line->new_lineno;
            }else{
                // Don't know what is this
                return 0;
            }

            hunks->push_back(hunk);
        }

        return 0;
    };

    rv = git_diff_index_to_workdir(&diff, gitState.repo, NULL, &diffopts);
    if(rv != 0){
        const git_error *err = git_error_last();
        DEBUG_MSG("Failed to compute diff %s\n", err->message);
    }else{
        rv = git_diff_foreach(diff, NULL, NULL, NULL, fn, (void *)&data);
        if(rv != 0){
            const git_error *err = git_error_last();
            DEBUG_MSG("Failed to compute diff %s\n", err->message);
        }
    }

    if(diff){
        git_diff_free(diff);
    }

    return rv == 0;
}

struct commit_info{
    git_time_t time;
    git_oid oid;
    std::string branch;
    git_commit *commit;
    int btype; // 0 - local only, 1 - remote only, 2 - both
    GitReferenceType type;
};

int commit_info_comp(commit_info *a, commit_info *b){
    return a->time > b->time;
}

typedef PriorityQueue<commit_info> CommitInfoPriorityQueue;

static CommitInfoPriorityQueue *Git_GetStartingCommits(bool local_only=false){
    CommitInfoPriorityQueue *pq = nullptr;
    if(gitState.repo){
        std::vector<commit_info *> list;
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

                bool insert = true;
                git_reference_name_to_id(&info->oid, gitState.repo, git_reference_name(ref));
                git_commit_lookup(&info->commit, gitState.repo, &info->oid);

                std::string h1 = Git_GetCommitHash(info->commit);
                for(commit_info *e : list){
                    std::string h0 = Git_GetCommitHash(e->commit);
                    insert = h0 != h1;
                    if(!insert) break;
                }

                if(insert){
                    info->type = type;
                    info->branch = branchName;
                    info->time = git_commit_time(info->commit);
                    PriorityQueue_Push(pq, info);
                    list.push_back(info);
                }else{
                    git_commit_free(info->commit);
                    delete info;
                }
            }
            return true;
        };

        Git_TransverseRelevantReferences(iterator);
    }
    return pq;
}

std::string shrink_message(std::string msg){
    int i = -1;
    for(uint j = 0; j < msg.size(); j++){
        if(msg[j] == '\n' && j > 0){
            i = j;
            break;
        }
    }

    return msg.substr(0, i);
}

static std::map<std::string, bool> expMap;
static int counter = 0;
void Git_PrintTree(git_commit *commit, int id){
    struct commit_v{
        git_commit *cmt;
        int it;
    };

    std::stack<commit_v> stack;
    stack.push({.cmt = commit, .it = id});
    while(stack.size() > 0){
        commit_v v = stack.top();
        stack.pop();

        git_commit *cmt = v.cmt;

        const git_oid *oid = git_commit_id(cmt);
        std::string buf(git_oid_tostr_s(oid), 9);

        if(expMap.find(buf) != expMap.end()){
            // nothing
        }else{
            std::string msg = shrink_message(git_commit_message(cmt));
            uint count = git_commit_parentcount(cmt);
            printf("[%d/%d] %s - %s\n", v.it, (int)count, buf.c_str(), msg.c_str());

            for(int i = count-1; i >= 0; i--){
                git_commit *p = nullptr;
                git_commit_parent(&p, cmt, i);
                stack.push({.cmt = p, .it = v.it+1});
            }

            expMap[buf] = true;
            counter++;
        }

        git_commit_free(cmt);
    }
}

void Git_PrintTree3(git_commit *commit, int id){
    const git_oid *oid = git_commit_id(commit);
    if(oid){
        const char *ps = git_oid_tostr_s(oid);
        std::string buf(ps, 9);
        std::string msg = shrink_message(git_commit_message(commit));

        if(expMap.find(buf) != expMap.end()) return;

        expMap[buf] = true;
        uint count = git_commit_parentcount(commit);
        printf("[%d/%d] %s - %s\n", id, (int)count, buf.c_str(), msg.c_str());
        counter++;

        for(uint i = 0; i < count; i++){
            git_commit *p = nullptr;
            git_commit_parent(&p, commit, i);

            Git_PrintTree3(p, id+1);
            git_commit_free(p);
        }
    }
}

void Git_PrintTree2(commit_info *info){
    git_commit *commit = info->commit;
    printf("================ %s\n", info->branch.c_str());
    while(commit != nullptr){
        git_commit *parent = nullptr;
        const git_oid *oid = git_commit_id(commit);
        std::string buf(git_oid_tostr_s(oid), 9);
        std::string msg = shrink_message(git_commit_message(commit));
        uint count = git_commit_parentcount(commit);
        if(count == 1){
            printf("* %s - %s\n", buf.c_str(), msg.c_str());
            git_commit_parent(&parent, commit, 0);
        }else if(count == 0){
            printf("* %s - %s\n", buf.c_str(), msg.c_str());
        }else{
            printf("* [%u] %s - %s\n", count, buf.c_str(), msg.c_str());
            git_commit_parent(&parent, commit, 0);
        }
        counter++;

        git_commit_free(commit);
        commit = parent;
    }
    printf("=================================================\n");
}

bool Git_LogGraph(){
    CommitInfoPriorityQueue *pq = Git_GetStartingCommits();
    if(!pq) return false;
    if(PriorityQueue_Size(pq) > 0){
        printf("Size %d\n", PriorityQueue_Size(pq));
        commit_info *info = PriorityQueue_Peek(pq);
        PriorityQueue_Pop(pq);

        //Git_PrintTree2(info);
        //Git_PrintTree3(info->commit, 0);
        //Git_PrintTree(info->commit, 0);
        //printf("Counter = %d\n", (int)counter);
        //GitGraph *graph = GitGraph_Build(info->commit);
        //GitGraph_Free(graph);
    }
#if 0
    while(PriorityQueue_Size(pq) > 0){
        commit_info *info = PriorityQueue_Peek(pq);
        PriorityQueue_Pop(pq);

        std::string buf(git_oid_tostr_s(&info->oid), 9);
        struct tm t;
        localtime_r(&info->time, &t);
        printf("%s %s ( %d/%d/%d : %d:%d:%d )\n", buf.c_str(), info->branch.c_str(),
                t.tm_mday, t.tm_mon+1, t.tm_year+1900, t.tm_hour, t.tm_min, t.tm_sec);

        git_commit_free(info->commit);
        delete info;
    }
#endif
    PriorityQueue_Free(pq);
    return true;
}

bool Git_ComputeCurrentDiff(){
    if(!gitState.repo || gitState.is_disabled) return false;

    // recompute diff
    if(gitState.diff){
        git_diff_free(gitState.diff);
        gitState.diff = nullptr;
    }

    git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
    int rv = git_diff_index_to_workdir(&gitState.diff, gitState.repo, NULL, &diffopts);
    if(rv != 0){
        DEBUG_MSG("Failed to get diff\n");
        return false;
    }

    return true;
}

std::string Git_GetRepositoryRoot(){
    if(!gitState.repo || gitState.is_disabled) return std::string();
    const char *gitp = git_repository_path(gitState.repo);
    std::string str(gitp);

    int id = GetRightmostSplitter(gitp, str.size()-1);
    if(id > 0){
        str = str.substr(0, id);
    }
    return str;
}

void Git_Initialize(){
    GIT_DISABLED_RET();
    git_libgit2_init();
    //DEBUG_MSG("Initialized\n");
}

void Git_Finalize(){
    GIT_DISABLED_RET();
    if(gitState.repo) git_repository_free(gitState.repo);
    if(gitState.head) git_reference_free(gitState.head);
    if(gitState.diff) git_diff_free(gitState.diff);
    git_libgit2_shutdown();
    //DEBUG_MSG("Finalize\n");
}

void Git_Disable(){
    gitState.is_disabled = true;
}

