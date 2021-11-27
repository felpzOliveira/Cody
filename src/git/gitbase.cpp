#include <gitbase.h>
#include <git2.h>
#include <iostream>
#include <app.h>
#include <types.h>
#include <map>

#define MODULE_NAME "GIT"

struct GitState{
    git_repository *repo;
    git_reference *head;
    git_diff *diff;
};

GitState gitState = {.repo = nullptr, .head = nullptr, .diff = nullptr};

static bool Git_TryOpenRepo(git_repository **repo, git_reference **head){
    int rv = 0;
    std::string root = AppGetRootDirectory();

    rv = git_repository_open(repo, root.c_str());
    if(rv != 0){
        DEBUG_MSG("Failed to open repository at [%s]\n", root.c_str());
        git_error_clear();
    }else{
        DEBUG_MSG("Opened repository at [%s]\n", root.c_str());
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

bool Git_OpenRootRepository(){
    bool rv = false;
    git_repository *repo = nullptr;
    git_reference *head = nullptr;
    if(Git_TryOpenRepo(&repo, &head)){
        if(gitState.head) git_reference_free(gitState.head);
        if(gitState.repo) git_repository_free(gitState.repo);

        gitState.repo = repo;
        gitState.head = head;
        rv = true;
    }

    return rv;
}

bool Git_GetReferenceHeadName(std::string &name){
    // check if the repo was successfully opened
    if(!gitState.head) return false;
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
            str = " New:        ";
        }else if((entry->status & GIT_STATUS_INDEX_MODIFIED) ||
                 (entry->status & GIT_STATUS_WT_MODIFIED))
        {
            str = " Modified:   ";
        }else if((entry->status & GIT_STATUS_INDEX_DELETED) ||
                 (entry->status & GIT_STATUS_WT_DELETED))
        {
            str = " Deleted:    ";
        }else if((entry->status & GIT_STATUS_INDEX_RENAMED) ||
                 (entry->status & GIT_STATUS_WT_RENAMED))
        {
            str = " Renamed:    ";
        }else if((entry->status & GIT_STATUS_INDEX_TYPECHANGE) ||
                 (entry->status & GIT_STATUS_WT_TYPECHANGE))
        {
            str = " Typechange: ";
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
    if(!gitState.repo) return false;
    if(!gitState.diff) return false;

    auto fn = [](const git_diff_delta *delta, float, void *obj) -> int{
        std::vector<std::string> *list = (std::vector<std::string> *)obj;
        if(list){
            if(!delta->new_file.path && delta->old_file.path){
                // file removed
                std::string val(" Removed:   ");
                val += delta->old_file.path;
                list->push_back(val);
            }else if(!delta->old_file.path && delta->new_file.path){
                std::string val(" New:   ");
                val += delta->old_file.path;
                list->push_back(val);
            }else if(delta->old_file.path && delta->new_file.path){
                std::string path(delta->new_file.path);
                std::string val(" Modified:   ");
                if(path != delta->old_file.path){
                    val = " Renamed:   ";
                }
                val += path;
                list->push_back(val);
            }else{
                // don't know what is this rename?
                printf("???\n");
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

bool Git_FetchDiffFor(const char *target, std::vector<GitDiffLine> *_hunks){
    if(!gitState.repo) return false;
    if(!gitState.diff) return false;

    struct diff_data{
        std::vector<GitDiffLine> *hks;
        std::string tgt;
    };

    diff_data data = {.hks = _hunks, .tgt = std::string(target), };

    auto fn = [](const git_diff_delta *delta, const git_diff_hunk *hunk,
                 const git_diff_line *line, void * obj) -> int
    {
        diff_data *dt = (diff_data *)obj;
        if(!delta->new_file.path) return 0;

        std::string hpath(delta->new_file.path);
        if(hpath == dt->tgt){
            GitDiffLine hunk;
            std::vector<GitDiffLine> *hunks = dt->hks;

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

    int rv = git_diff_foreach(gitState.diff, NULL, NULL, NULL, fn, (void *)&data);
    if(rv != 0){
        DEBUG_MSG("Failed to compute diff\n");
    }

    return rv == 0;
}

bool Git_ComputeCurrentDiff(){
    if(!gitState.repo) return false;

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

void Git_Initialize(){
    git_libgit2_init();
    DEBUG_MSG("Initialized\n");
}

void Git_Finalize(){
    if(gitState.repo) git_repository_free(gitState.repo);
    if(gitState.head) git_reference_free(gitState.head);
    git_libgit2_shutdown();
    DEBUG_MSG("Finalize\n");
}


