#ifndef TRANS_H
#define TRANS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <getopt.h>
#include <signal.h>

#define BUFFER_SIZE 4096
#define MAX_ENCODED_SIZE (BUFFER_SIZE * 4)

typedef enum {
    METHOD_UUENCODE,
    METHOD_ESCAPE
} encode_method_t;

typedef enum {
    MODE_RECEIVER,
    MODE_SENDER
} trans_mode_t;

typedef struct {
    trans_mode_t mode;
    int port;
    encode_method_t method;
    char *host;
    char *system_command;
    int quiet;
} config_t;

// エンコード/デコード関数
size_t uuencode_data(const unsigned char *input, size_t input_len, char *output);
size_t uudecode_data(const char *input, size_t input_len, unsigned char *output);
size_t escape_encode_data(const unsigned char *input, size_t input_len, char *output);
size_t escape_decode_data(const char *input, size_t input_len, unsigned char *output);

// メイン機能
int sender_mode(const config_t *config);
int receiver_mode(const config_t *config);
void handle_connection(int sockfd, const config_t *config);

// グローバル変数
extern volatile int running;

// ユーティリティ
void parse_arguments(int argc, char *argv[], config_t *config);
void print_usage(const char *program_name);
void cleanup_and_exit(int sig);

#endif
