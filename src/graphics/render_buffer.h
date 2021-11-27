/* date = November 22nd 2021 19:41 */
#pragma once
#include <buffers.h>
#include <gitbase.h>
#include <gitbuffer.h>
#if 0
typedef enum{
    RENDERBUFFER_LINEBUFFER=0,
    RENDERBUFFER_GITLINEBUFFER=1,
    RENDERBUFFER_NONE=2
}RenderBufferType;

class RenderableLineBuffer{
    public:
    LineBuffer *lineBuffer;
    RenderableLineBuffer(): lineBuffer(nullptr){}
    RenderableLineBuffer(LineBuffer *linebuffer) : lineBuffer(linebuffer){}

    void Set(LineBuffer *linebuffer){ lineBuffer = linebuffer; }

    virtual int GetRequiredLineCount(){ return lineBuffer->lineCount; }
    virtual Buffer *GetBufferAt(uint at){ return LineBuffer_GetBufferAt(lineBuffer, at); }
    virtual LineBuffer *GetBaseLineBuffer(){ return lineBuffer; }
    virtual RenderBufferType GetRenderBufferType(){ return RENDERBUFFER_LINEBUFFER; }
    virtual ~RenderableLineBuffer(){}
};

class RenderableGitLineBuffer : public RenderableLineBuffer{
    public:
    GitLineBuffer *gitLineBuffer;
    std::vector<GitDiffLine> *difs;
    uint totalLines;
    Buffer ubuffer;

    RenderableGitLineBuffer() : RenderableLineBuffer(), gitLineBuffer(nullptr),
        difs(nullptr), totalLines(0)
    {
        Buffer_Init(&ubuffer, DefaultAllocatorSize);
    }

    RenderableGitLineBuffer(GitLineBuffer *gitBuf, std::vector<GitDiffLine> *_difs):
        RenderableLineBuffer(gitBuf->base), gitLineBuffer(gitBuf), difs(_difs),
        totalLines(0)
    {
        Buffer_Init(&ubuffer, DefaultAllocatorSize);
        ComputeLines();
    }

    RenderableGitLineBuffer(GitLineBuffer *gitBuf): RenderableLineBuffer(gitBuf->base),
        gitLineBuffer(gitBuf), difs(nullptr), totalLines(0)
    {
        Buffer_Init(&ubuffer, DefaultAllocatorSize);
    }

    void Set(GitLineBuffer *gitLb){ gitLineBuffer = gitLb; }

    void SetDiffs(std::vector<GitDiffLine> *diffs){
        difs = diffs;
        ComputeLines();
    }

    void ComputeLines(){
        uint n = gitLineBuffer->base->lineCount;
        for(GitDiffLine line : *difs){
            n += line.ctype == GIT_LINE_REMOVED;
        }

        totalLines = n;
    }

    virtual int GetRequiredLineCount() override{ return totalLines; }

    virtual Buffer *GetBufferAt(uint at) override{
        // check if there is enough information for this query
        if(at >= totalLines){
            printf("Invalid query id %u\n", at);
            return nullptr;
        }

        // check if there is no diff, trivial case
        if(difs->size() == 0){
            return LineBuffer_GetBufferAt(gitLineBuffer->base, at);
        }

        // check if the request lies prior to the first hunk, trivial case
        GitDiffLine line = difs->at(0);
        if(line.lineno > 0){
            if((uint)line.lineno > at+1){
                Buffer *buf = LineBuffer_GetBufferAt(gitLineBuffer->base, at);
                printf("%s\n", buf->data);
                return buf;
            }
        }

        // lies after the first hunk, compute new index
        uint additions = 0;
        uint removed = 0;
        // we need to compute the real position of the removed lines in
        // the current view of the file. A line could be removed at position 30
        // but if there was 3 insertions befores than this line should be
        // rendered at position 33 instead.
        for(GitDiffLine gLine : *difs){
            if(gLine.lineno < 0) continue;

            uint p = gLine.lineno;
            if(gLine.ctype == GIT_LINE_REMOVED){
                p += additions;
                removed += 1;
            }else if(gLine.ctype == GIT_LINE_INSERTED){
                additions += 1;
                p += removed;
            }

            // in case at < p, the line we were targetting lies in
            // the original linebuffer with position at - removed
            if(at+1 < p){
                Buffer *buf = LineBuffer_GetBufferAt(gitLineBuffer->base, at - removed);
                printf("%s\n", buf->data);
                return nullptr;
                //return LineBuffer_GetBufferAt(gitLineBuffer->base, at - removed);
            }else if(at+1 == p){ // lies on a hunk
                if(gLine.ctype == GIT_LINE_REMOVED){
                    printf("-%s\n", gLine.content.c_str());
                    Buffer_RemoveRange(&ubuffer, 0, ubuffer.count);
                    Buffer_InsertStringAt(&ubuffer, 0, (char *)gLine.content.c_str(),
                                          gLine.content.size(), 1);
                    Buffer_FastTokenGen(&ubuffer);
                    return &ubuffer;
                }else{
                    // Use the linebuffer value as it is tokenized
                    Buffer *buf = LineBuffer_GetBufferAt(gitLineBuffer->base, at - removed);
                    printf("+%s\n", buf->data);
                    return buf;
                }
            }
        }

        // lies out of all hunks, simply remove the removed lines and return result
        Buffer *buf2 = LineBuffer_GetBufferAt(gitLineBuffer->base, at - removed);
        printf("%s\n", buf2->data);
        return buf2;
        //return LineBuffer_GetBufferAt(gitLineBuffer->base, at - removed);
    }

    virtual LineBuffer *GetBaseLineBuffer() override{
        return gitLineBuffer->base;
    }

    virtual RenderBufferType GetRenderBufferType() override{
        return RENDERBUFFER_GITLINEBUFFER;
    }

    virtual ~RenderableGitLineBuffer(){
        Buffer_Free(&ubuffer);
    }
};

class RenderBuffer{
    public:
    RenderableLineBuffer renderBufferLinebuffer;
    RenderableGitLineBuffer renderBufferGitLinebuffer;
    RenderBufferType type;

    RenderBuffer(){
        type = RENDERBUFFER_NONE;
    }

    Buffer *GetBufferAt(uint at){
        if(type == RENDERBUFFER_LINEBUFFER){
            return renderBufferLinebuffer.GetBufferAt(at);
        }else if(type == RENDERBUFFER_GITLINEBUFFER){
            return renderBufferGitLinebuffer.GetBufferAt(at);
        }else{
            return nullptr;
        }
    }

    int GetRequiredLineCount(){
        if(type == RENDERBUFFER_LINEBUFFER){
            return renderBufferLinebuffer.GetRequiredLineCount();
        }else if(type == RENDERBUFFER_GITLINEBUFFER){
            return renderBufferGitLinebuffer.GetRequiredLineCount();
        }else{
            return 0;
        }
    }

    RenderableLineBuffer *GetRenderableLineBuffer(){
        return &renderBufferLinebuffer;
    }

    RenderableGitLineBuffer *GetRenderableGitLineBuffer(){
        return &renderBufferGitLinebuffer;
    }

    void Set(LineBuffer *lineBuffer){
        type = RENDERBUFFER_LINEBUFFER;
        renderBufferLinebuffer.Set(lineBuffer);
    }

    void Set(GitLineBuffer *gitLb){
        type = RENDERBUFFER_GITLINEBUFFER;
        renderBufferGitLinebuffer.Set(gitLb);
    }

    void Set(std::vector<GitDiffLine> *difs){
        type = RENDERBUFFER_GITLINEBUFFER;
        renderBufferGitLinebuffer.SetDiffs(difs);
    }

    ~RenderBuffer(){

    }
};
#endif
