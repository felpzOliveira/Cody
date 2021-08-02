// based on https://www.cppstories.com/2019/04/dir-iterate
#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <vector>

namespace fs = std::filesystem;

std::string path_without_ext(std::string path){
    size_t index = path.find_last_of(".");
    if(index == std::string::npos) return path;

    return path.substr(0, index);
}

template<typename Fn>
int ForAllFiles(const char *dir, const Fn &fn){
    const fs::path targetPath{ dir };
    int r = 0;
    for(const auto &entry : fs::directory_iterator(targetPath)){
        const auto filename = entry.path().filename().string();
        if(entry.is_directory()){

        }else if(entry.is_regular_file()){
            std::ifstream ifs(entry.path());
            if(ifs.is_open()){
                r = fn(entry, filename, &ifs);
                ifs.close();
            }else{
                std::cout << "Failed to open " << filename << std::endl;
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
        FILE *fp = fopen(entry.path().c_str(), "r");
        if(!fp){
            return -1;
        }

        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        rewind(fp);
        unsigned char *buffer = new unsigned char[size];

        long n = fread(buffer, 1, size, fp);
        int rv = -1;
        if(n == size){
            rv = 0;
            std::string ptmp("// Instanced from ");
            ptmp += entry.path();
            int len = ptmp.size() - 1;
            ss << "/"; for(int k = 0; k < len; k++) ss << "*"; ss << "/\n";
            ss << ptmp << std::endl;
            ss << "/"; for(int k = 0; k < len; k++) ss << "*"; ss << "/\n";

            ss << "unsigned int " << filekey << "_len = " << size << ";\n";
            ss << "unsigned char "<< filekey << "[] = {\n";
            //printf("================== %s => %ld\n", entry.path().c_str(), size);
            char mem[16];
            for(int i = 0; i < size; i++){
                snprintf(mem, sizeof(mem), "%s%x%s",
                         (int)buffer[i] <= 0xF ? "0x0" : "0x",
                         (int)buffer[i], (i+1) % 12 == 0 ? ",\n" : ", ");
                ss << std::string(mem);
            }
            ss << " };" << std::endl;
            std::string v("extern unsigned char "); v += filekey;
            v += "[];";
            vars->push_back(v);

            v = std::string("extern unsigned int "); v += filekey;
            v += "_len;";
            vars->push_back(v);
        }

        delete[] buffer;
        fclose(fp);
        return rv;
    });

    if(r != 0) return r;
    std::string f = ss.str();

    if(f.size() == 0){
        std::cout << "No icons found\n" << std::endl;
        return -1;
    }

    c_file += std::string("/const_icons.cpp");
    std::ofstream ofsc_file(c_file);
    if(ofsc_file.is_open()){
        ofsc_file << f;
        ofsc_file.close();
    }else{
        std::cout << "Failed to open output file" << std::endl;
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
        ptmp += entry.path();
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
    });

    if(r != 0){
        std::cout << "Error reading directory" << std::endl;
        return -1;
    }

    std::string f = ss.str();

    if(f.size() == 0){
        std::cout << "No shader code found\n" << std::endl;
        return -1;
    }

    c_file += std::string("/const_shader.cpp");
    std::ofstream ofsc_file(c_file);
    if(ofsc_file.is_open()){
        ofsc_file << f;
        ofsc_file.close();
    }else{
        std::cout << "Failed to open output file" << std::endl;
        return -1;
    }

    return 0;
}

int main(int argc, char **argv){
    std::string c_file("../src/resources");
    if(argc != 3 && argc != 4){
        std::cout << "No paths specified" << std::endl;
        return -1;
    }else if(argc == 4){
        c_file = std::string(argv[3]);
    }

    std::vector<std::string> vars;
    std::string resourcepath(argv[2]);
    resourcepath += "/resources.h";

    int r = PackShaders(&vars, argv[1], c_file);
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
        std::ofstream res_ofs(resourcepath);
        if(res_ofs.is_open()){
            res_ofs << res_str;
        }else{
            std::cout << "Failed to open resource files" << std::endl;
            return -1;
        }
    }

    return 0;
}
