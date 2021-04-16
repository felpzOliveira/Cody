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

static void CppHeaderCreate(LineBuffer *lineBuffer, Tokenizer *tokenizer){
    std::stringstream ss;
    std::time_t curr = time(0);
    std::tm *now = std::localtime(&curr);
    std::string ext = "th ";
    uint digit = GetDigitOf(now->tm_mday, 0); // equivalently to tm_mday % 10

    if(digit == 1 && now->tm_mday != 11){
        ext = "st ";
    }else if(digit == 2){
        ext = "nd ";
    }else if(digit == 3){
        ext = "rd ";
    }

    ss << "/* date = " << months[now->tm_mon] << " " << now->tm_mday << ext <<
          (now->tm_year + 1900) << " " << now->tm_hour << ":" << now->tm_min << " */";

    std::string str = ss.str();
    LineBuffer_InsertLineAt(lineBuffer, 0, (char *)str.c_str(), str.size(), 0);
    LineBuffer_ReTokenizeFromBuffer(lineBuffer, tokenizer, 0, 0);
}

static void Hook_OnCreate(char *path, uint len, LineBuffer *lineBuffer, Tokenizer *tokenizer){
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

static void Hook_OnOpen(char *path, uint len, LineBuffer *lineBuffer, Tokenizer *tokenizer){
    printf("[HOOK] Default file open hook: %s\n", path);
}

void FileHooks_RegisterDefault(){
    RegisterFileOpenHook(Hook_OnOpen);
    RegisterFileCreateHook(Hook_OnCreate);
}
