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
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/select.h>

#define BUFFER_SIZE 256
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
    char *log_port_stdio_file;
    char *log_stdio_port_file;
} config_t;

// エンコード/デコード関数
size_t uuencode_data(const unsigned char *input, size_t input_len, unsigned char *output);
size_t uudecode_data(const unsigned char *input, size_t input_len, unsigned char *output);
size_t escape_encode_data(const unsigned char *input, size_t input_len, unsigned char *output);
size_t escape_decode_data(const unsigned char *input, size_t input_len, unsigned char *output);

// メイン機能
int sender_mode(const config_t *config);
int receiver_mode(const config_t *config);
void handle_connection(int sockfd, const config_t *config);
ssize_t read_with_timeout(int fd, void *buffer, size_t count, int timeout_ms);

// グローバル変数
extern volatile int running;

// ユーティリティ
void parse_arguments(int argc, char *argv[], config_t *config);
void print_usage(const char *program_name);
void cleanup_and_exit(int sig);
void hex_dump_to_file(FILE *file, const char *prefix, const unsigned char *data, size_t len);

#endif
