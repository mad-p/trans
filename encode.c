#include "trans.h"
#include <ctype.h>

size_t uuencode_data(const unsigned char *input, size_t input_len, char *output) {
    size_t i, j = 0;
    size_t bytes_processed = 0;
    
    while (bytes_processed < input_len) {
        size_t line_bytes = (input_len - bytes_processed > 45) ? 45 : (input_len - bytes_processed);
        
        // 行の長さを書く
        output[j++] = ' ' + line_bytes;
        
        for (i = 0; i < line_bytes; i += 3) {
            unsigned char c1 = input[bytes_processed + i];
            unsigned char c2 = (bytes_processed + i + 1 < input_len) ? input[bytes_processed + i + 1] : 0;
            unsigned char c3 = (bytes_processed + i + 2 < input_len) ? input[bytes_processed + i + 2] : 0;
            
            output[j++] = ' ' + ((c1 >> 2) & 0x3f);
            output[j++] = ' ' + (((c1 << 4) & 0x30) | ((c2 >> 4) & 0x0f));
            output[j++] = ' ' + (((c2 << 2) & 0x3c) | ((c3 >> 6) & 0x03));
            output[j++] = ' ' + (c3 & 0x3f);
        }
        
        output[j++] = '\n';
        bytes_processed += line_bytes;
    }
    
    if (input_len == 0) {
        output[j++] = ' ';
        output[j++] = '\n';
    }
    
    output[j] = '\0';
    return j;
}

size_t uudecode_data(const char *input, size_t input_len, unsigned char *output) {
    size_t i = 0, j = 0;
    
    while (i < input_len) {
        if (input[i] == '\n') {
            i++;
            continue;
        }
        
        if (i >= input_len) break;
        
        int line_len = input[i] - ' ';
        i++;
        
        if (line_len < 0 || line_len > 45) {
            // 行をスキップ
            while (i < input_len && input[i] != '\n') {
                i++;
            }
            continue;
        }
        
        int decoded = 0;
        while (decoded < line_len && i + 3 < input_len) {
            unsigned char c1 = input[i] - ' ';
            unsigned char c2 = input[i + 1] - ' ';
            unsigned char c3 = input[i + 2] - ' ';
            unsigned char c4 = input[i + 3] - ' ';
            
            if (decoded < line_len) {
                output[j++] = ((c1 << 2) & 0xfc) | ((c2 >> 4) & 0x03);
                decoded++;
            }
            if (decoded < line_len) {
                output[j++] = ((c2 << 4) & 0xf0) | ((c3 >> 2) & 0x0f);
                decoded++;
            }
            if (decoded < line_len) {
                output[j++] = ((c3 << 6) & 0xc0) | (c4 & 0x3f);
                decoded++;
            }
            
            i += 4;
        }
        
        // 行の残りをスキップ
        while (i < input_len && input[i] != '\n') {
            i++;
        }
    }
    
    return j;
}

size_t escape_encode_data(const unsigned char *input, size_t input_len, char *output) {
    size_t i, j = 0;
    
    for (i = 0; i < input_len; i++) {
        unsigned char c = input[i];
        
        if (c == 0x0d || c == 0x0a || c == 0x1c || c == 0x7f || c == 0x5c) {
            output[j++] = 0x5c;
            sprintf(&output[j], "%02x", c);
            j += 2;
        } else {
            output[j++] = c;
        }
    }
    
    output[j] = '\0';
    return j;
}

size_t escape_decode_data(const char *input, size_t input_len, unsigned char *output) {
    size_t i = 0, j = 0;
    
    while (i < input_len) {
        if (input[i] == 0x5c && i + 2 < input_len) {
            char hex_str[3];
            hex_str[0] = input[i + 1];
            hex_str[1] = input[i + 2];
            hex_str[2] = '\0';
            
            if (isxdigit(hex_str[0]) && isxdigit(hex_str[1])) {
                unsigned int byte_val;
                sscanf(hex_str, "%02x", &byte_val);
                output[j++] = (unsigned char)byte_val;
                i += 3;
            } else {
                output[j++] = input[i++];
            }
        } else {
            output[j++] = input[i++];
        }
    }
    
    return j;
}