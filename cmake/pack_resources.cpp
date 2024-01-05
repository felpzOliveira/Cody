// based on https://www.cppstories.com/2019/04/dir-iterate
#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <map>

typedef unsigned int uint;

#define LOG_MSG(...)do{\
    FILE *__fp = fopen("pack.log", "a+");\
    if(__fp){\
        fprintf(__fp, __VA_ARGS__);\
        fclose(__fp);\
    }\
}while(0)

namespace fs = std::filesystem;

int starts_with(std::string s0, std::string s1){
    if(s1.size() > s0.size()) return 0;
    for(uint i = 0; i < s1.size(); i++){
        if(s0[i] != s1[i]) return 0;
    }

    return 1;
}

void split(std::string s, std::vector<std::string> &output, char value){
    std::string val;
    bool on_space = false;
    for(unsigned int i = 0; i < s.size(); i++){
        if(s[i] == value){
            if(!on_space){
                on_space = true;
                if(val.size() > 0){
                    output.push_back(val);
                    val.clear();
                }
            }
        }else{
            on_space = false;
            val.push_back(s[i]);
        }
    }

    if(val.size() > 0){
        output.push_back(val);
    }
}

std::string clean_string(std::string str){
    const std::string WHITESPACE = " \n\r\t\f\v";
    auto lefttrim = [&](std::string s) -> std::string{
        size_t start = s.find_first_not_of(WHITESPACE);
        return (start == std::string::npos) ? "" : s.substr(start);
    };

    auto righttrim = [&](std::string s) -> std::string{
        size_t end = s.find_last_not_of(WHITESPACE);
        return (end == std::string::npos) ? "" : s.substr(0, end + 1);
    };

    str = righttrim(lefttrim(str));
    std::string::iterator siterator = std::unique(str.begin(), str.end(),
    [](char el, char er)->bool{
        return (el == er) && (el == ' ');
    });

    str.erase(siterator, str.end());
    return str;
}

std::string path_without_ext(std::string path){
    size_t index = path.find_last_of(".");
    if(index == std::string::npos) return path;

    return path.substr(0, index);
}

std::string read_file(std::string path){
    std::string resp;
    FILE *fp = fopen(path.c_str(), "rb");
    if(fp){
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        char *mem = new char[size];
        rewind(fp);
        long n = fread(mem, 1, size, fp);
        resp = std::string(mem, n);
        delete[] mem;
        fclose(fp);
    }

    return resp;
}

template<typename Fn>
int ForAllFiles(const char *dir, const Fn &fn, int debug=0){
    const fs::path targetPath{ dir };
    int r = 0;
    for(const auto &entry : fs::directory_iterator(targetPath)){
        const auto filename = entry.path().filename().string();
        if(entry.is_directory()){

        }else if(entry.is_regular_file()){
            if(debug){
                std::string value =
                        read_file(reinterpret_cast<const char*>(entry.path().c_str()));
                std::cout << "******************** READ:" << std::endl;
                std::cout << value << std::endl;
            }
            std::ifstream ifs(entry.path());
            if(ifs.is_open()){
                r = fn(entry, filename, &ifs);
                ifs.close();
            }else{
                LOG_MSG("Failed to open %s\n", filename.c_str());
                r = -1;
            }

            if(r != 0) break;
        }else{
            r = -1;
            break;
        }
    }

    return r;
}

int PackIcons(std::vector<std::string> *vars, std::string baseDir, std::string buildDir){
    std::string c_file = buildDir;
    std::string dir = baseDir + std::string("/icons");
    std::stringstream ss;
    ss << "/* Auto generated file ( " << __DATE__ << " " << __TIME__ << " ) */\n\n";

    int r = ForAllFiles(dir.c_str(), [&](fs::directory_entry entry,
                        std::string filenameStr, std::ifstream *ifs) -> int
    {
        std::string filekey = filenameStr;
        std::replace(filekey.begin(), filekey.end(), '.', '_');
        // re-open file for byte handling
        std::string value = dir + "/" + filenameStr;
        std::ifstream inputFile(value, std::ios::binary);
        if(!inputFile.is_open()){
            return -1;
        }

        inputFile.seekg(0, std::ios::end);
        std::streampos fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        std::vector<unsigned char> bufferVec(fileSize);
        if(!inputFile.read(reinterpret_cast<char*>(bufferVec.data()), fileSize)){
            return -1;
        }

        unsigned char *buffer = bufferVec.data();

        if(1){
            std::string ptmp("// Instanced from ");
            ptmp += value;
            int len = ptmp.size() - 1;
            ss << "/"; for(int k = 0; k < len; k++) ss << "*"; ss << "/\n";
            ss << ptmp << std::endl;
            ss << "/"; for(int k = 0; k < len; k++) ss << "*"; ss << "/\n";

            ss << "unsigned int " << filekey << "_len = " << fileSize << ";\n";
            ss << "unsigned char "<< filekey << "[] = {\n";
            //printf("================== %s => %ld\n", entry.path().c_str(), size);
            char mem[16];
            for(int i = 0; i < fileSize; i++){
                snprintf(mem, sizeof(mem), "%s%x%s",
                         (int)buffer[i] <= 0xF ? "0x0" : "0x",
                         (int)buffer[i], (i+1) % 12 == 0 ? ",\n" : ", ");
                ss << std::string(mem);
            }
            ss << " };\n" << std::endl;
            std::string v("extern unsigned char "); v += filekey;
            v += "[];";
            vars->push_back(v);

            v = std::string("extern unsigned int "); v += filekey;
            v += "_len;";
            vars->push_back(v);
        }

        return 0;
    });

    if(r != 0) return r;
    std::string f = ss.str();

    if(f.size() == 0){
        LOG_MSG("No icons found\n");
        return -1;
    }

    c_file += std::string("/const_icons.cpp");
    std::ofstream ofsc_file(c_file, std::ios::binary);
    if(ofsc_file.is_open()){
        ofsc_file << f;
        ofsc_file.close();
    }else{
        LOG_MSG("Failed to open output file");
        return -1;
    }

    return 0;
}

int PackShaders(std::vector<std::string> *vars, std::string baseDir, std::string buildDir){
    std::string c_file = buildDir;
    std::string dir = baseDir + std::string("/shaders");
    std::stringstream ss;
    ss << "/* Auto generated file ( " << __DATE__ << " " << __TIME__ << " ) */\n";
    ss << "#define MULTILINE_STRING(a) #a\n";
    ss << "#define MULTILINE_STRING_V(v, a) \"#\" #v \"\\n\" #a\n\n\n";
    int r = ForAllFiles(dir.c_str(), [&](fs::directory_entry entry,
                        std::string filenameStr, std::ifstream *ifs) -> int
    {
        std::string filekey = path_without_ext(filenameStr);
        std::replace(filekey.begin(), filekey.end(), '.', '_');

        std::string filecontent;
        std::string line;
        std::string versionstr;
        int first_line = 1;
        while(std::getline(*ifs, line)){
            if(line.size() > 0){
                if(first_line){
                    size_t index = line.find("#version");
                    if(index != std::string::npos){
                        versionstr = line.substr(index + 1);
                    }else{
                        filecontent += line + std::string("\n");
                    }
                    first_line = 0;
                }else{
                    filecontent += line + std::string("\n");
                }
            }
        }

        std::string ptmp("// Instanced from ");
        ptmp += reinterpret_cast<const char*>(entry.path().c_str());
        int len = ptmp.size() - 1;
        ss << "/"; for(int k = 0; k < len; k++) ss << "*"; ss << "/\n";
        ss << ptmp << std::endl;
        ss << "/"; for(int k = 0; k < len; k++) ss << "*"; ss << "/\n";
        ss << "const char *shader_" << filekey << " = ";
        if(versionstr.size() > 0){
            ss << "MULTILINE_STRING_V(" << versionstr << ",\n";
        }else{
            ss << "MULTILINE_STRING(\n";
        }
        ss << filecontent << ");\n\n";
        std::string v("extern const char *");
        v += std::string("shader_") + filekey; v += ";";
        vars->push_back(v);

        return 0;
    }, 0);

    if(r != 0){
        LOG_MSG("Error reading directory\n");
        return -1;
    }

    std::string f = ss.str();

    if(f.size() == 0){
        LOG_MSG("No shader code found\n");
        return -1;
    }

    c_file += std::string("/const_shader.cpp");
    std::ofstream ofsc_file(c_file, std::ios::binary);
    if(ofsc_file.is_open()){
        ofsc_file << f;
        ofsc_file.close();
    }else{
        LOG_MSG("Failed to open output file\n");
        return -1;
    }

    return 0;
}

int language_table_gen(std::string path){
    struct token_type{
        std::string value;
        std::string type;

        token_type(std::string v, std::string t){
            value = v;
            type = t;
        }
    };

    constexpr unsigned int split_count = 2;
    std::string line;

    std::map<std::string, std::map<int, std::vector<token_type>>> tokenNamedMaps;

    std::ifstream ifs(path);
    if(!ifs.is_open()){
        LOG_MSG(" ** Could not open %s\n", path.c_str());
        return -1;
    }

    bool parsing = false;
    std::string runningName;
    std::string pathName;
    std::map<int, std::vector<token_type>> runningMap;
    std::string context;

    while(std::getline(ifs, line)){
        if(!parsing){
            if(starts_with(line, "BEGIN ")){
                runningName = line.substr(6);
                runningMap.clear();
                parsing = true;
            }else if(starts_with(line, "TARGET ")){
                pathName = std::string("../") + line.substr(7);
            }else{
                context += line + "\n";
            }
        }else if(parsing){
            if(line == "END"){
                tokenNamedMaps[runningName] = runningMap;
                parsing = false;
            }else{
                std::vector<std::string> splitted;
                split(line, splitted, ' ');
                if(splitted.size() != split_count){
                    LOG_MSG(" ** Invalid line %s\n", line.c_str());
                    ifs.close();
                    return -1;
                }

                std::string val = clean_string(splitted[0]);
                std::string tp  = clean_string(splitted[1]);
                if(runningMap.find(val.size()) == runningMap.end()){
                    std::vector<token_type> tk;
                    tk.push_back(token_type(val, tp));
                    runningMap[val.size()] = tk;
                }else{
                    runningMap[val.size()].push_back(token_type(val, tp));
                }
            }
        }
    }

    ifs.close();

    std::stringstream ss;
    ss << "/* Build from '" << path << "' */\n";
    ss << "/* Contextual content */\n";
    ss << "/////////////////////////////////\n";
    ss << context << "\n";
    ss << "/////////////////////////////////\n";

    ss << "/* Auto generated file ( " << __DATE__ << " " << __TIME__ << " ) */\n";
    for(auto bit = tokenNamedMaps.begin(); bit != tokenNamedMaps.end(); bit++){
        ss << "\nstd::vector<std::vector<GToken>> " << bit->first << " = {\n";
        for(auto it = bit->second.begin(); it != bit->second.end(); it++){
            unsigned int count = it->second.size();
            unsigned int v = 0;
            ss << "    {\n";
            for(token_type tk : it->second){
                ss << "        { .value = \"" << tk.value << "\", .identifier = TOKEN_ID_"
                    << tk.type << " },";
                if(v < count-1){
                    ss << "\n";
                }else{
                    ss << " },\n";
                }
                v++;
            }
        }
        ss << "};\n";
    }

    if(pathName.size() > 0){
        std::ofstream ofs(pathName, std::ios::binary);
        if(ofs.is_open()){
            ofs << ss.str() << "\n";
            ofs.close();
        }
    }
    return 0;
}

int PackLanguages(std::string baseDir){
    std::string dir = baseDir + std::string("/lang_tables");
    int r = ForAllFiles(dir.c_str(), [&](fs::directory_entry entry,
                        std::string filenameStr, std::ifstream *ifs) -> int
    {
        std::string path = dir + std::string("/") + filenameStr;
        return language_table_gen(path.c_str());
    });
    return r;
}

int main(int argc, char **argv){
    std::string c_file("../src/resources");
    if(argc != 3 && argc != 4){
        LOG_MSG("No paths specified\n");
        return -1;
    }else if(argc == 4){
        //c_file = std::string(argv[3]);
    }

    std::vector<std::string> vars;
    std::string resourcepath(argv[2]);
    resourcepath += "/resources.h";

    int r = PackShaders(&vars, argv[1], c_file);
    if(r != 0) return r;

    r = PackLanguages(argv[1]);
    if(r != 0) return r;

    r = PackIcons(&vars, argv[1], c_file);
    if(r != 0) return r;

    std::ifstream res_ifs(resourcepath);
    std::string res_str;
    int added = 0;

    if(res_ifs.is_open()){
        res_str = std::string((std::istreambuf_iterator<char>(res_ifs)),
                               std::istreambuf_iterator<char>());

        res_ifs.close();
    }

    if(res_str.size() == 0 && vars.size() > 0) res_str = "#pragma once";

    for(std::string &v : vars){
        size_t index = res_str.find(v);
        if(index == std::string::npos){
            res_str += std::string("\n");
            res_str += v;
            added = 1;
        }
    }

    if(added){
        std::ofstream res_ofs(resourcepath, std::ios::binary);
        if(res_ofs.is_open()){
            res_ofs << res_str;
        }else{
            LOG_MSG("Failed to open resource files\n");
            return -1;
        }
    }

    return 0;
}
