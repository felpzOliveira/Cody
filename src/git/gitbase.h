/* date = November 19th 2021 19:58 */
#pragma once
#include <string>
#include <vector>

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

// Make the text strings global so we can easily detect which
// operation is performed on a git status
#define GIT_LABEL_SIZE           14
#define GIT_NEW_FILE_STR         " New:        "
#define GIT_MODIFIED_FILE_STR    " Modified:   "
#define GIT_DELETED_FILE_STR     " Deleted:    "
#define GIT_RENAMED_FILE_STR     " Renamed:    "
#define GIT_TYPECHANGE_FILE_STR  " Typechange: "

/*
 * Initializes the git interface.
 */
void Git_Initialize();

/*
 * Terminates the git interface.
 */
void Git_Finalize();

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
