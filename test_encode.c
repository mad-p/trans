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
    size_t remaining_bytes;
    size_t decoded_len = escape_decode_data(encoded, encoded_len, decoded, &remaining_bytes);
    
    assert(decoded_len == sizeof(input1));
    assert(memcmp(input1, decoded, sizeof(input1)) == 0);
    printf("  Test 1 passed: Special characters encoding/decoding\n");
    
    // テストケース2: 通常の文字列
    unsigned char input2[] = "Hello, World!";
    size_t input2_len = strlen((char*)input2);
    
    encoded_len = escape_encode_data(input2, input2_len, encoded);
    decoded_len = escape_decode_data(encoded, encoded_len, decoded, &remaining_bytes);
    
    assert(decoded_len == input2_len);
    assert(memcmp(input2, decoded, input2_len) == 0);
    printf("  Test 2 passed: Normal string encoding/decoding\n");
    
    // テストケース3: 空のデータ
    encoded_len = escape_encode_data(NULL, 0, encoded);
    decoded_len = escape_decode_data(encoded, encoded_len, decoded, &remaining_bytes);
    
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
    size_t remaining_bytes;
    size_t decoded_len = uudecode_data(encoded, encoded_len, decoded, &remaining_bytes);
    
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
    decoded_len = uudecode_data(encoded, encoded_len, decoded, &remaining_bytes);
    
    assert(decoded_len == sizeof(input2));
    assert(memcmp(input2, decoded, sizeof(input2)) == 0);
    printf("  Test 2 passed: Binary data uuencoding/decoding\n");
    
    // テストケース3: 3で割り切れない長さのデータ
    unsigned char input3[] = "Hello, World!";
    size_t input3_len = strlen((char*)input3);
    
    encoded_len = uuencode_data(input3, input3_len, encoded);
    decoded_len = uudecode_data(encoded, encoded_len, decoded, &remaining_bytes);
    
    assert(decoded_len == input3_len);
    assert(memcmp(input3, decoded, input3_len) == 0);
    printf("  Test 3 passed: Non-multiple of 3 length data\n");
}

void test_large_data() {
    printf("Testing large data...\n");
    
    // 大きなデータのテスト (BUFFER_SIZEまで)
    unsigned char large_input[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; i++) {
        large_input[i] = i % 256;
    }
    
    char encoded[MAX_ENCODED_SIZE];
    unsigned char decoded[BUFFER_SIZE];
    size_t remaining_bytes;
    
    // エスケープエンコードテスト
    size_t encoded_len = escape_encode_data(large_input, BUFFER_SIZE, encoded);
    size_t decoded_len = escape_decode_data(encoded, encoded_len, decoded, &remaining_bytes);
    
    printf("  Encoded %zu bytes to %zu bytes, decoded %zu bytes, remaining %zu\n", 
           (size_t)BUFFER_SIZE, encoded_len, decoded_len, remaining_bytes);
    
    if (decoded_len != BUFFER_SIZE || remaining_bytes != 0) {
        printf("  First 20 bytes of original: ");
        for (int i = 0; i < 20 && i < BUFFER_SIZE; i++) printf("%02x ", large_input[i]);
        printf("\n");
        printf("  First 20 bytes of decoded: ");
        for (int i = 0; i < 20 && i < decoded_len; i++) printf("%02x ", decoded[i]);
        printf("\n");
    }
    
    assert(decoded_len == BUFFER_SIZE);
    assert(remaining_bytes == 0);
    assert(memcmp(large_input, decoded, BUFFER_SIZE) == 0);
    printf("  Large data escape encode/decode passed\n");
    
    // uuencodeテスト
    encoded_len = uuencode_data(large_input, BUFFER_SIZE, encoded);
    decoded_len = uudecode_data(encoded, encoded_len, decoded, &remaining_bytes);
    
    assert(decoded_len == BUFFER_SIZE);
    assert(remaining_bytes == 0);
    assert(memcmp(large_input, decoded, BUFFER_SIZE) == 0);
    printf("  Large data uuencode/decode passed\n");
}

void test_buffer_boundary() {
    printf("Testing buffer boundary issues...\n");
    
    // エスケープシーケンスのバッファ境界テスト
    printf("  Testing escape sequence buffer boundary...\n");
    
    // テストケース1: バッファ末尾で \ だけ
    unsigned char escape_partial1[] = {'H', 'e', 'l', 'l', 'o', 0x5c};
    unsigned char decoded[BUFFER_SIZE];
    size_t remaining_bytes;
    size_t decoded_len = escape_decode_data(escape_partial1, sizeof(escape_partial1), decoded, &remaining_bytes);
    
    assert(decoded_len == 5); // "Hello" の部分のみデコード
    assert(remaining_bytes == 1); // \ が残る
    assert(memcmp(decoded, "Hello", 5) == 0);
    printf("    Partial escape (\\) at buffer end: passed\n");
    
    // テストケース2: バッファ末尾で \x だけ
    unsigned char escape_partial2[] = {'H', 'e', 'l', 'l', 'o', 0x5c, '0'};
    decoded_len = escape_decode_data(escape_partial2, sizeof(escape_partial2), decoded, &remaining_bytes);
    
    assert(decoded_len == 5); // "Hello" の部分のみデコード
    assert(remaining_bytes == 2); // \0 が残る
    assert(memcmp(decoded, "Hello", 5) == 0);
    printf("    Partial escape (\\x) at buffer end: passed\n");
    
    // テストケース3: 完全なエスケープシーケンス
    unsigned char escape_complete[] = {'H', 'e', 'l', 'l', 'o', 0x5c, '0', 'd'};
    decoded_len = escape_decode_data(escape_complete, sizeof(escape_complete), decoded, &remaining_bytes);
    
    assert(decoded_len == 6); // "Hello" + 0x0d
    assert(remaining_bytes == 0); // 残りなし
    assert(memcmp(decoded, "Hello\r", 6) == 0);
    printf("    Complete escape sequence: passed\n");
    
    // uuencodeのバッファ境界テスト
    printf("  Testing uuencode buffer boundary...\n");
    
    // 不完全な行のテスト
    unsigned char uu_partial[] = {' ' + 5, ' ' + 20, ' ' + 21}; // 行長5、不完全な4バイト組
    decoded_len = uudecode_data(uu_partial, sizeof(uu_partial), decoded, &remaining_bytes);
    
    assert(decoded_len == 0); // デコードされたデータなし
    assert(remaining_bytes == 3); // 全体が残る
    printf("    Incomplete uuencode line: passed\n");
    
    printf("  All buffer boundary tests passed\n");
}

int main() {
    printf("Running encoding/decoding unit tests...\n\n");
    
    test_escape_encode_decode();
    printf("\n");
    
    test_uuencode_decode();
    printf("\n");
    
    test_large_data();
    printf("\n");
    
    test_buffer_boundary();
    printf("\n");
    
    printf("All tests passed!\n");
    return 0;
}