#include <file_provider.h>
#include <utilities.h>
#include <string>
#include <sstream>
#include <ctime>
#include <buffers.h>

std::string months[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

void CppHeaderCreate(LineBuffer *lineBuffer, Tokenizer *tokenizer){
    std::stringstream ss;
    std::time_t curr = time(0);
    std::tm *now = std::localtime(&curr);

    ss << "/* date = " << months[now->tm_mon] << " " << now->tm_mday << "th " <<
          (now->tm_year + 1900) << " " << now->tm_hour << ":" << now->tm_min << " */";

    std::string str = ss.str();
    LineBuffer_InsertLineAt(lineBuffer, 0, (char *)str.c_str(), str.size(), 0);
    LineBuffer_ReTokenizeFromBuffer(lineBuffer, tokenizer, 0, 0);
}

void Hook_OnCreate(char *path, uint len, LineBuffer *lineBuffer, Tokenizer *tokenizer){
    int p = GetFilePathExtension(path, len);
    if(p > 0){
        char *ext = &path[p];
        std::string strExt(ext);
        // TODO: Check all file extensions
        if(strExt == ".h" || strExt == ".hpp"){
            CppHeaderCreate(lineBuffer, tokenizer);
        }
    }
}

void Hook_OnOpen(char *path, uint len, LineBuffer *lineBuffer, Tokenizer *tokenizer){
    printf("[HOOK] Default file open hook: %s\n", path);
}

void FileHooks_RegisterDefault(){
    FileProvider_RegisterFileOpenHook(Hook_OnOpen);
    FileProvider_RegisterFileCreateHook(Hook_OnCreate);
}
