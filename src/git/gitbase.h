/* date = November 19th 2021 19:58 */
#pragma once
#include <string>
#include <vector>
#include <git2.h>
#include <time.h>
#include <utilities.h>

// these don't really belong in here...
typedef enum{
    GIT_LINE_REMOVED = 0,
    GIT_LINE_INSERTED,
}LineTypes;

struct LineHighlightInfo{
    int lineno;
    LineTypes ctype;
    std::string content;
};

// the state of the current repo opened
struct GitState{
    git_repository *repo;
    git_reference *head;
    git_diff *diff;
    bool is_disabled;
};

// Utilities for fetching commits
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

// Make the text strings global so we can easily detect which
// operation is performed on a git status
#define GIT_LABEL_SIZE           14
#define GIT_NEW_FILE_STR         " New:        "
#define GIT_MODIFIED_FILE_STR    " Modified:   "
#define GIT_DELETED_FILE_STR     " Deleted:    "
#define GIT_RENAMED_FILE_STR     " Renamed:    "
#define GIT_TYPECHANGE_FILE_STR  " Typechange: "

struct commit_info{
    git_time_t time;
    git_oid oid;
    std::string branch;
    git_commit *commit;
    int btype; // 0 - local only, 1 - remote only, 2 - both
    GitReferenceType type;
};

inline int commit_info_comp(commit_info *a, commit_info *b){
    return a->time > b->time;
}

inline const char *GitReferenceString(GitReferenceType type){
    switch(type){
        case GIT_REF_TAG : return "Tag";
        case GIT_REF_BRANCH_LOCAL : return "Local Branch";
        case GIT_REF_BRANCH_REMOTE : return "Remote Branch";
        default: return "Unknow";
    }
}

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

// The function fn should return true in case it wants to keep transversing
// or false if it should interrupt
template<typename Fn> inline
bool Git_TransverseRelevantReferences(GitState *state, const Fn &fn){
    if(!state->repo) return false;
    int rv = 0, err = 0;
    git_reference_iterator *it = nullptr;
    git_reference *ref = nullptr;
    bool keep = true;

    rv = git_reference_iterator_new(&it, state->repo);
    if(rv != 0){
        printf("Failed to create references iterator\n");
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

/*
 * Initializes the git interface.
 */
void Git_Initialize();

/*
 * Terminates the git interface.
 */
void Git_Finalize();

/*
* Disable all git functions.
*/
void Git_Disable();

/*
 * Attempt to open the root directory of the application as a git repo.
 */
bool Git_OpenRootRepository();

/*
* Returns the root directory of the opened repository. This should always
* match the app root directory, but it is possible a combination of 'git open'
* made this change so explicitly allow app to get the git root. In case no root
* was opened this routine returns an empty string.
*/
std::string Git_GetRepositoryRoot();

/*
 * Attempts to open the given path as a git repository.
 */
bool Git_OpenDirectory(char *path);

/*
 * Get the head reference string.
 */
bool Git_GetReferenceHeadName(std::string &name);

/*
 * Compute the diff from the current working tree and update internal
 * git state. Must be called before fetching diff data.
 */
bool Git_ComputeCurrentDiff();

/*
 * Given a target (file) of the working tree, get all hunks from the previously
 * computed diff.
 */
bool Git_FetchDiffFor(const char *target, std::vector<LineHighlightInfo> *hunks);

/*
 * Compute all deltas (files) of the working tree and return in an array.
 * This is to simulate diff --name-only.
 */
bool Git_FetchDiffDeltas(std::vector<std::string> *_deltas);

/*
* Compute status of the working dir. Returns a list of strings with
* file status (new file, modified, removed, ...).
*/
bool Git_FetchStatus(std::vector<std::string> *_status);

/*
* Retrieves the graph representation of log. Similar to running:
*     git log --graph
*/
bool Git_LogGraph();



