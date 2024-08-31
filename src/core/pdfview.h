/* date = June 7th 2024 16:11 */
#pragma once
#include <types.h>
#include <geometry.h>

struct PdfScrollState{
    int page_off;
    int active;
};

struct PdfViewGraphicState{
    vec2f zoomCenter;
    Float zoomLevel;
    int pageIndex;
};

struct PdfViewState;

/*
* Checks if we can call poppler.
*/
bool PdfView_IsEnabled();

/*
* Opens a pdf document into a pdfview. Must be called before everything else.
* Receives the content 'pdf' and its size 'len' but also the path and pathLen
* This is done because bufferviews dont store paths (only linebuffers) and if we
* wish to index this document we need to store its path.
*/
bool PdfView_OpenDocument(PdfViewState **pdfView, const char *pdf, uint len,
                          const char *path, uint pathLen);

/*
* Checks if the scroll is active in the pdfView.
*/
bool PdfView_IsScrolling(PdfViewState *pdfView);

/*
* Free the resources taken by the document.
*/
void PdfView_CloseDocument(PdfViewState **pdfView);

/*
* Get the total number of pages inside the pdfview.
*/
int PdfView_GetNumPages(PdfViewState *pdfView);

/*
* Gets a reference to the pdf scroll info.
*/
PdfScrollState *PdfView_GetScroll(PdfViewState *pdfView);

/*
* Gets const acessor to the pixels of the current opened page.
*/
const char *PdfView_GetCurrentPage(PdfViewState *pdfView, int &width, int &height);

/*
* Sets the document to be on a specific page. Update internals to reflect that.
*/
void PdfView_OpenPage(PdfViewState *pdfView, int pageIndex);

/*
* Swapps to the next page.
*/
void PdfView_NextPage(PdfViewState *pdfView);

/*
* Swaps to the previous page.
*/
void PdfView_PreviousPage(PdfViewState *pdfView);

/*
* Jumps n pages in any direction, positive values go forward, negative backwards.
*/
void PdfView_JumpNPages(PdfViewState *pdfView, int n);

/*
* Estimate the page number active _now_.
*/
int PdfView_EstimateScrollPage(PdfViewState *pdfView);

/*
* Clears internal change flag.
*/
void PdfView_ClearPendingFlag(PdfViewState *pdfView);

/*
* Checks for pending changes.
*/
bool PdfView_Changed(PdfViewState *pdfView);

/*
* Projects the coordinate 'point' into the given geometry considering the
* aspect ratio of the current opened page.
* NOTE(1): This routine works in normalized spaces, i.e.: point must be in (0,1)
*          and the output will also be in (0,1).
* NOTE(2): This routine assumes that the page is being analysed with proper aspect
*          ratio, i.e.: it is properly being scaled.
*/
bool PdfView_ProjectCoordinate(PdfViewState *pdfView, vec2f point,
                               Geometry *geometry, vec2f &proj);

/*
* Handles mouse motion. Updates the zoom center variable so that viewing is centered.
*/
void PdfView_MouseMotion(PdfViewState *pdfView, vec2f mouse,
                         int is_pressed, Geometry *geometry);

/*
* Sets the position of the zoom center, this must be in (0,1) range.
*/
void PdfView_SetZoomCenter(PdfViewState *pdfView, vec2f center);

/*
* Gets the zoom center as last updated.
*/
vec2f PdfView_GetZoomCenter(PdfViewState *pdfView);

/*
* Gets the zoom level as last updated.
*/
Float PdfView_GetZoomLevel(PdfViewState *pdfView);

/*
* Controls the zoom level.
*/
void PdfView_IncreaseZoomLevel(PdfViewState *pdfView);
void PdfView_DecreaseZoomLevel(PdfViewState *pdfView);

/*
* Controls zoom locking.
*/
void PdfView_UnlockZoom(PdfViewState *pdfView);
void PdfView_LockZoom(PdfViewState *pdfView);
bool PdfView_IsZoomLocked(PdfViewState *pdfView);

/*
* Reloads the current opened document.
*/
bool PdfView_Reload(PdfViewState **pdfView);

/*
* Gets some properties from the pdfView so it can persist across reloads.
*/
PdfViewGraphicState PdfView_GetGraphicsState(PdfViewState *pdfView);

/*
* Sets the graphic state and page for the document. This is going to take
* the document to whatever page is given in the state.
*/
void PdfView_SetGraphicsState(PdfViewState *pdfView, PdfViewGraphicState state);

/*
* Enables the internal zoom control.
*/
void PdfView_EnableZoom(PdfViewState *pdfView);

/*
* Disables internal zoom control.
*/
void PdfView_DisableZoom(PdfViewState *pdfView);

/*
* Resets zoom variables.
*/
void PdfView_ResetZoom(PdfViewState *pdfView);

/*
* Gets a const accessor to the pixels of the page 'pageIndex'. This function
* does not update the internal pageIndex, kinda of a random access. Exposing just
* for tests, shouldn't actually be called elsewhere.
*/
const char *PdfView_GetImagePage(PdfViewState *pdfView, int pageIndex,
                                 int &width, int &height);

/*
* Checks if the pdf view can move to position given by 'center'. This checks that
* the rendering of the box (0,0)x(1,1) is bounded considering the current zoom level.
*/
bool PdfView_CanMoveTo(PdfViewState *pdfView, vec2f center);
