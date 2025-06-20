#include <iostream>
#include <graphics.h>
#include <utilities.h>
#include <parallel.h>
#include <app.h>
#include <file_provider.h>
#include <cryptoutil.h>
#include <arg_parser.h>
#include <storage.h>
#include <security_services.h>
#include <modal.h>
#include <rng.h>
#include <buffers.h>
#include <map>
#include <set>

void LineBuffer_LoopAllTokens(LineBuffer *lineBuffer){
    uint n = lineBuffer->lineCount;
    uint count = 0;
    std::map<int, std::set<std::string>> tokenMap;
    std::set<std::string> resultSet;
    for(uint i = 0; i < n; i++){
        Buffer *buffer = lineBuffer->lines[i];
        for(uint k = 0; k < buffer->tokenCount; k++){
            Token *token = &buffer->tokens[k];
            if(!Lex_IsUserToken(token))
                continue;

            char m = buffer->data[token->position + (uint)token->size];
            buffer->data[token->position + (uint)token->size] = 0;

            std::string val =
                    std::string(&buffer->data[token->position], token->size);
            /*
            if(token->identifier == TOKEN_ID_PREPROCESSOR_DEFINITION){
                printf("%s = ( %s )\n", val.c_str(),
                    Symbol_GetIdString(token->identifier));
            }
            */

            tokenMap[token->identifier].insert(
                val
            );

            buffer->data[token->position + (uint)token->size] = m;
        }
    }

    std::set<std::string> &user_data =
            tokenMap[TOKEN_ID_DATATYPE_USER_DATATYPE];
    std::set<std::string> &user_struct =
            tokenMap[TOKEN_ID_DATATYPE_USER_STRUCT];
    std::set<std::string> &user_enums =
            tokenMap[TOKEN_ID_DATATYPE_USER_ENUM_VALUE];
    std::set<std::string> &user_defs =
            tokenMap[TOKEN_ID_PREPROCESSOR_DEFINITION];

    std::string output;
    for(std::string str : user_enums){
        auto rv = resultSet.insert(str);
        if(rv.second){
            output += str + " RESERVED\n";
        }
    }

    for(std::string str : user_data){
        auto rv = resultSet.insert(str);
        if(rv.second){
            output += str + " DATATYPE\n";
        }
    }

    for(std::string str : user_struct){
        auto rv = resultSet.insert(str);
        if(rv.second){
            output += str + " DATATYPE\n";
        }
    }

    for(std::string str : user_defs){
        auto rv = resultSet.insert(str);
        if(rv.second){
            output += str + " RESERVED\n";
        }
    }

    if(output.size() == 0){
        std::cout << "Lexer consumed everything, nothing to output" << std::endl;
    }else{
        std::cout << "Possibly non-supported types:" << std::endl;
        std::cout << output;
    }
}

int main(int argc, char **argv){
    uint fileSize = 0;
    ENABLE_MODAL_MODE = true;
    LEX_DISABLE_PROC_STACK = false;

    if(argc != 2){
        std::cout << "Usage " << argv[0] << " <input_file>" << std::endl;
        return 0;
    }

    const char *targetPath = argv[1];

    StorageDeviceEarlyInit();
    Crypto_InitRNGEngine();
    SetStorageDevice(StorageDeviceType::Local);
    DebuggerRoutines();
    AppEarlyInitialize(0);

    LineBuffer *lineBuffer = nullptr;
    Tokenizer cppTokenizer;
    SymbolTable symbolTable;

    char *fileContents = GetFileContents(targetPath, &fileSize);
    if(!fileContents){
        std::cout << "Could not read file " << targetPath << std::endl;
        return 0;
    }

    //std::cout << "Read " << targetPath << std::endl;

    int fileType = 0;
    FileProvider_Load((char *)targetPath, (uint)strlen(targetPath),
                      fileType, &lineBuffer, true);
    //LineBuffer_Init(&lineBuffer, &cppTokenizer, fileContents, fileSize, true);

    //LineBuffer_DebugPrintRange(lineBuffer, vec2i(0, lineBuffer->lineCount));
    LineBuffer_LoopAllTokens(lineBuffer);
    return 0;
}
