#include "trans.h"
#include <assert.h>
#include <string.h>

void test_escape_encode_decode() {
    printf("Testing escape encode/decode...\n");
    
    // テストケース1: 特殊文字を含むデータ
    unsigned char input1[] = {0x0d, 0x0a, 0x1c, 0x7f, 0x5c, 'H', 'e', 'l', 'l', 'o'};
    char encoded[MAX_ENCODED_SIZE];
    unsigned char decoded[BUFFER_SIZE];
    
    size_t encoded_len = escape_encode_data(input1, sizeof(input1), encoded);
    size_t decoded_len = escape_decode_data(encoded, encoded_len, decoded);
    
    assert(decoded_len == sizeof(input1));
    assert(memcmp(input1, decoded, sizeof(input1)) == 0);
    printf("  Test 1 passed: Special characters encoding/decoding\n");
    
    // テストケース2: 通常の文字列
    unsigned char input2[] = "Hello, World!";
    size_t input2_len = strlen((char*)input2);
    
    encoded_len = escape_encode_data(input2, input2_len, encoded);
    decoded_len = escape_decode_data(encoded, encoded_len, decoded);
    
    assert(decoded_len == input2_len);
    assert(memcmp(input2, decoded, input2_len) == 0);
    printf("  Test 2 passed: Normal string encoding/decoding\n");
    
    // テストケース3: 空のデータ
    encoded_len = escape_encode_data(NULL, 0, encoded);
    decoded_len = escape_decode_data(encoded, encoded_len, decoded);
    
    assert(encoded_len == 0);
    assert(decoded_len == 0);
    printf("  Test 3 passed: Empty data encoding/decoding\n");
}

void test_uuencode_decode() {
    printf("Testing uuencode/uudecode...\n");
    
    // テストケース1: 基本的な文字列
    unsigned char input1[] = "Hello";
    char encoded[MAX_ENCODED_SIZE];
    unsigned char decoded[BUFFER_SIZE];
    
    size_t input1_len = strlen((char*)input1);
    size_t encoded_len = uuencode_data(input1, input1_len, encoded);
    size_t decoded_len = uudecode_data(encoded, encoded_len, decoded);
    
    printf("  Input: '%s' (len=%zu)\n", (char*)input1, input1_len);
    printf("  Encoded: '");
    for (size_t k = 0; k < encoded_len; k++) {
        if (encoded[k] == '\n') printf("\\n");
        else printf("%c", encoded[k]);
    }
    printf("' (len=%zu)\n", encoded_len);
    printf("  Decoded len: %zu\n", decoded_len);
    if (decoded_len > 0) {
        printf("  Decoded: '");
        for (size_t k = 0; k < decoded_len; k++) {
            printf("%c", decoded[k]);
        }
        printf("'\n");
    }
    
    assert(decoded_len == input1_len);
    assert(memcmp(input1, decoded, input1_len) == 0);
    printf("  Test 1 passed: Basic string uuencoding/decoding\n");
    
    // テストケース2: バイナリデータ
    unsigned char input2[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0xff, 0xfe, 0xfd};
    
    encoded_len = uuencode_data(input2, sizeof(input2), encoded);
    decoded_len = uudecode_data(encoded, encoded_len, decoded);
    
    assert(decoded_len == sizeof(input2));
    assert(memcmp(input2, decoded, sizeof(input2)) == 0);
    printf("  Test 2 passed: Binary data uuencoding/decoding\n");
    
    // テストケース3: 3で割り切れない長さのデータ
    unsigned char input3[] = "Hello, World!";
    size_t input3_len = strlen((char*)input3);
    
    encoded_len = uuencode_data(input3, input3_len, encoded);
    decoded_len = uudecode_data(encoded, encoded_len, decoded);
    
    assert(decoded_len == input3_len);
    assert(memcmp(input3, decoded, input3_len) == 0);
    printf("  Test 3 passed: Non-multiple of 3 length data\n");
}

void test_large_data() {
    printf("Testing large data...\n");
    
    // 大きなデータのテスト
    unsigned char large_input[1000];
    for (int i = 0; i < 1000; i++) {
        large_input[i] = i % 256;
    }
    
    char encoded[MAX_ENCODED_SIZE];
    unsigned char decoded[BUFFER_SIZE];
    
    // エスケープエンコードテスト
    size_t encoded_len = escape_encode_data(large_input, 1000, encoded);
    size_t decoded_len = escape_decode_data(encoded, encoded_len, decoded);
    
    assert(decoded_len == 1000);
    assert(memcmp(large_input, decoded, 1000) == 0);
    printf("  Large data escape encode/decode passed\n");
    
    // uuencodeテスト
    encoded_len = uuencode_data(large_input, 1000, encoded);
    decoded_len = uudecode_data(encoded, encoded_len, decoded);
    
    assert(decoded_len == 1000);
    assert(memcmp(large_input, decoded, 1000) == 0);
    printf("  Large data uuencode/decode passed\n");
}

int main() {
    printf("Running encoding/decoding unit tests...\n\n");
    
    test_escape_encode_decode();
    printf("\n");
    
    test_uuencode_decode();
    printf("\n");
    
    test_large_data();
    printf("\n");
    
    printf("All tests passed!\n");
    return 0;
}