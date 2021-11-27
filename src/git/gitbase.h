/* date = November 19th 2021 19:58 */
#pragma once
#include <string>
#include <vector>

typedef enum{
    GIT_LINE_REMOVED = 0,
    GIT_LINE_INSERTED,
}GitLineTypes;

struct GitDiffLine{
    int lineno;
    GitLineTypes ctype;
    std::string content;
};

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
bool Git_FetchDiffFor(const char *target, std::vector<GitDiffLine> *hunks);

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
