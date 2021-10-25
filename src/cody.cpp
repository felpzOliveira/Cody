#include <iostream>
#include <graphics.h>
#include <utilities.h>
#include <string.h>
#include <unistd.h>
#include <app.h>
#include <file_provider.h>
#include <stdlib.h>
#include <stdio.h>
#include <bignum.h>
#include <rng.h>
#include <cryptoutil.h>
#include <aes.h>

void LoadStaticFilesOnStart(){
    std::string path = AppGetConfigFilePath();
    FILE *fp = fopen(path.c_str(), "r");
    if(fp){
        char *line = nullptr;
        size_t lineSize = 0;
        int read = -1;
        char folder[PATH_MAX];
        FileEntry entry;
        std::string rootPath = AppGetRootDirectory();
        while((read = getline(&line, &lineSize, fp)) != -1){
            if(read > 0){
                line[read-1] = 0; // remove \n
                std::string p = rootPath + std::string("/") + std::string(line);

                if(AppIsStoredFile(p)) continue;

                int r = GuessFileEntry((char *)p.c_str(), p.size(), &entry, folder);
                if(!(r < 0) && entry.type == DescriptorFile){
                    FileProvider_Load((char *)p.c_str(), p.size());
                    AppAddStoredFile(line);
                }
            }
        }
        fclose(fp);
    }
}

void StartWithFile(const char *path=nullptr){
    BufferView *bView = AppGetActiveBufferView();
    BufferView_Initialize(bView, nullptr);

    if(path != nullptr){
        LineBuffer *lineBuffer = nullptr;
        uint length = strlen(path);

        FileProvider_Load((char *)path, length, &lineBuffer, false);
        BufferView_SwapBuffer(bView, lineBuffer);
    }else{
        LoadStaticFilesOnStart();
    }

    Graphics_Initialize();
}

void CommandExecutorInit();
int main(int argc, char **argv){
    Bignum bn, bn2, c;
    Bignum_FromString(&bn,
        "6613F26162223DF488E9CD48CC132C7A"
        "0AC93C701B001B092E4E5B9F73BCD27B"
        "9EE50D0657C77F374E903CDFA4C642", 16);

    Bignum_FromString(&bn2, "1", 16);

    Bignum_Add(&c, &bn, &bn2);

    char str[4096];
    size_t len = 4096;

    Bignum_ToString(&bn, str, len, &len, 10);

    std::cout << str << std::endl;

    len = 4096;
    Bignum_ToString(&c, str, len, &len, 10);
    std::cout << str << std::endl;


    Bignum d;

    Bignum_Sub(&d, &bn, &c);
    len = 4096;
    Bignum_ToString(&d, str, len, &len, 10);
    std::cout << str << std::endl;

    size_t rsize = 32;
    unsigned char buffer[32];
    if(Crypto_SecureRNG(buffer, rsize)){
        printf("Got rng:\n");
        size_t i = 0;
        for(i = 0; i < rsize; i++){
            int v = (int)buffer[i];
            printf("%s%X%s", v <= 0x0F ? "0x0" : "0x", v, (i+1) % 16 == 0 ? "\n": " ");
        }

        if((i+1) % 16 != 0){
            printf("\n");
        }
    }

    std::string val;
    std::vector<unsigned char> out;
    std::string v("49276d206b696c6c696e6720796f757220627261696e206c696b65206120706f69736f6e6f7573206d757368726f6f6d");
    char *p = (char *)v.data();
    printf("--------------------------------------------------\n");
    CryptoUtil_BufferFromHex(out, p, v.size());
    {
        size_t i = 0;
        for(i = 0; i < out.size(); i++){
            int v = (int)out[i];
            printf("%s%X%s", v <= 0x0F ? "0x0" : "0x", v, (i+1) % 16 == 0 ? "\n": " ");
        }

        if((i+1) % 16 != 0){
            printf("\n");
        }
    }

    CryptoUtil_BufferToBase64(out.data(), out.size(), val);

    std::cout << val << std::endl;

    std::string ns;
    CryptoUtil_BufferToHex(out.data(), out.size(), ns);

    if(ns == v){
        printf("EQUALS\n");
    }else{
        printf("NOT EQUALS\n");
    }

    uint8_t buf[32];
    AES_GenerateKey(buf, AES256);

    return 0;

    DebuggerRoutines();

    CommandExecutorInit();

    if(argc > 1){
        char folder[PATH_MAX];
        char *p = argv[1];
        uint len = strlen(p);
        FileEntry entry;
        int r = GuessFileEntry(p, len, &entry, folder);

        if(r == -1){
            printf("Unknown argument\n");
            AppEarlyInitialize();
            StartWithFile();
        }

        IGNORE(CHDIR(folder));
        AppEarlyInitialize();

        if(entry.type == DescriptorFile){
            StartWithFile(entry.path);
        }else{
            StartWithFile();
        }

    }else{
        AppEarlyInitialize();
        StartWithFile();
    }

    return 0;
}
