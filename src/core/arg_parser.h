/* date = May 26th 2022 20:25 */
#pragma once
#include <map>
#include <stdio.h>

#define ARG_ABORT 0
#define ARG_OK    1

#define ARGUMENT_PROCESS(name)\
    int name(int argc, char **argv, int &i, const char *arg, void *config)

typedef ARGUMENT_PROCESS(ArgProcess);

typedef struct{
    ArgProcess *processor;
    std::string help;
}ArgDesc;

inline
void PrintHelpAndQuit(const char *caller, std::map<const char *, ArgDesc> argument_map){
    std::map<const char *, ArgDesc>::iterator it;
    printf("[ %s ] Help:\n", caller);
    for(it = argument_map.begin(); it != argument_map.end(); it++){
        ArgDesc desc = it->second;
        printf("  %s : %s\n", it->first, desc.help.c_str());
    }
    exit(0);
}

inline void ArgumentProcess(std::map<const char *, ArgDesc> argument_map,
                            int argc, char **argv, const char *caller, void *config,
                            std::function<bool(std::string)> unknownArgFn,
                            int enforce=1, int start=1)
{
    int argCount = argc - start;
    if(argCount > 0){
        for(int i = start; i < argc; i++){
            int ok = ARG_ABORT;
            int solved = 0;
            std::string arg(argv[i]);
            std::map<const char *, ArgDesc>::iterator it;
            if(arg == "--help" || arg == "help"){
                PrintHelpAndQuit(caller, argument_map);
            }
            for(it = argument_map.begin(); it != argument_map.end(); it++){
                if(arg == it->first){
                    ArgDesc desc = it->second;
                    ok = desc.processor(argc, argv, i, it->first, config);
                    solved = 1;
                    break;
                }
            }

            if(ok != ARG_OK && solved == 1){
                printf("Failed processing \'%s\'\n", argv[i]);
                exit(0);
            }else if(!solved){
                if(unknownArgFn(arg) != ARG_OK){
                    printf("Don't know what to do with %s\n", argv[i]);
                    exit(0);
                }

                ok = ARG_OK;
            }

            /* if return != 0 stop parsing (probably jumped to command) */
            if(ok != ARG_OK){ break; }
        }
    }else if(enforce){
        printf("Missing argument.\n");
        PrintHelpAndQuit(caller, argument_map);
    }
}

inline std::string ParseNext(int argc, char **argv, int &i, const char *arg, int count=1){
    int ok = (argc > i+count) ? 1 : 0;
    if(!ok){
        printf("Invalid argument for %s\n", arg);
        exit(0);
    }

    std::string res;
    for(int n = 1; n < count+1; n++){
        res += std::string(argv[n+i]);
        if(n < count) res += " ";
    }

    i += count;
    return res;
}

template<typename Fn>
inline int ParseNextOrNone(int argc, char **argv, int &i, const char *arg, Fn func){
    int ok = (argc > i + 1) ? 1 : 0;
    if(!ok) return 0;
    std::string res(argv[i+1]);
    int r = func(res);
    if(r){
        i += 1;
    }

    return r;
}

inline Float ParseNextFloat(int argc, char **argv, int &i, const char *arg){
    std::string value = ParseNext(argc, argv, i, arg);
    const char *token = value.c_str();
    return (Float)atof(token);
}
