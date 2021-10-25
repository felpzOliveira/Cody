#include <cryptoutil.h>

static std::string b64table = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz"
                              "0123456789+/";

void CryptoUtil_RotateBufferLeft(unsigned char *buffer, size_t len){
    unsigned char e0 = buffer[0];
    for(size_t i = 0; i < len-1; i++){
        buffer[i] = buffer[i+1];
    }

    buffer[len-1] = e0;
}

void CryptoUtil_RotateBufferRight(unsigned char *buffer, size_t len){
    unsigned char e0 = buffer[len-1];
    for(size_t i = len-1; i > 0; i--){
        buffer[i] = buffer[i-1];
    }

    buffer[0] = e0;
}

void CryptoUtil_BufferToHex(unsigned char *buffer, size_t size, std::string &str){
    char hex[3];
    for(size_t i = 0; i < size; i++){
        int val = buffer[i];
        snprintf(hex, 3, "%02x", val);
        str += std::string(hex);
    }
}

void CryptoUtil_BufferToBase64(unsigned char *buffer, size_t size, std::string &str){
    // 3 bytes of data are splitted into 4 6-bit values each becomes a symbol
    // that is looked up on the table
    unsigned char bytes[4];
    unsigned char source[3];
    int i = 0;
    while(size--){
        source[i++] = *(buffer++);
        if(i == 3){
            bytes[0] = (source[0] & 0xFC) >> 2;
            bytes[1] = ((source[0] & 0x03) << 4) | ((source[1] & 0xF0) >> 4);
            bytes[2] = ((source[1] & 0x0F) << 2) | ((source[2] & 0x60) >> 6);
            bytes[3] = (source[2] & 0x3F);

            for(i = 0; i < 4; i++){
                str += b64table[bytes[i]];
            }

            i = 0;
        }
    }

    // padding
    if(i != 0){
        // push 0
        for(int j = i; j < 3; j++){
            source[j] = 0;
        }

        bytes[0] = (source[0] & 0xFC) >> 2;
        bytes[1] = ((source[0] & 0x03) << 4) | ((source[1] & 0xF0) >> 4);
        bytes[2] = ((source[1] & 0x0F) << 2) | ((source[2] & 0x60) >> 6);
        bytes[3] = (source[2] & 0x3F);

        for(int j = 0; j < i+1; j++){
            str += b64table[bytes[j]];
        }

        while(i++ < 3){
            str += "=";
        }
    }
}

void CryptoUtil_BufferFromHex(std::vector<unsigned char> &out, char *data, size_t len){
    std::string hex(data, len);
    for(size_t i = 0; i < len; i += 2){
        std::string byteString = hex.substr(i, 2);
        char byte = (char) strtol(byteString.c_str(), NULL, 16);
        out.push_back(byte);
    }
}

void CryptoUtil_BufferFromBase64(std::vector<unsigned char> &out, char *data, size_t len){
    // 4 bytes from string are merged into 3 bytes of data
    unsigned char source[4];
    unsigned char bytes[3];
    int i = 0, c = 0;

    if(len % 4 != 0){
        printf("Base64 encoded data is not multiple of 4\n");
    }

    while(len--){
        source[i] = data[c++];
        i++;

        if(i == 4){
            // get indexes from values
            for(i = 0; i < 4; i++){
                source[i] = b64table.find(source[i]);
            }

            bytes[0] = ((source[0] & 0x3F) << 2) | ((source[1] & 0x30) >> 4);
            bytes[1] = ((source[1] & 0x0F) << 4) | ((source[2] & 0x3C) >> 2);
            bytes[2] = ((source[2] & 0x03) << 6) | source[3];

            for(i = 0; i < 3; i++){
                out.push_back(bytes[i]);
            }

            i = 0;
        }
    }

    if(i != 0){ // does this makes sense?
        // push 0
        for(int j = i; j < 3; j++){
            source[j] = 0;
        }

        for(int j = 0; j < 4; j++){
            source[j] = b64table.find(source[j]);
        }

        bytes[0] = ((source[0] & 0x3F) << 2) | ((source[1] & 0x30) >> 4);
        bytes[1] = ((source[1] & 0x0F) << 4) | ((source[2] & 0x3C) >> 2);
        bytes[2] = ((source[2] & 0x03) << 6) | source[3];

        for(i = 0; i < 3; i++){
            out.push_back(bytes[i]);
        }
    }
}
