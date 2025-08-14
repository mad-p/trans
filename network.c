#include "trans.h"

ssize_t read_with_timeout(int fd, void *buffer, size_t count, int timeout_ms) {
    struct pollfd pfd;
    int poll_result;
    int original_flags;
    ssize_t result;
    
    // ファイルディスクリプタをノンブロッキングモードに設定
    original_flags = fcntl(fd, F_GETFL, 0);
    if (original_flags == -1) {
        return -1;
    }
    if (fcntl(fd, F_SETFL, original_flags | O_NONBLOCK) == -1) {
        return -1;
    }
    
    // pollで読み取り可能になるまで待機
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    
    poll_result = poll(&pfd, 1, timeout_ms);
    
    if (poll_result < 0) {
        // pollエラー
        fcntl(fd, F_SETFL, original_flags); // フラグを復元
        return -1;
    } else if (poll_result == 0) {
        // タイムアウト
        fcntl(fd, F_SETFL, original_flags); // フラグを復元
        return 0;
    }
    
    // データが読み取り可能
    if (pfd.revents & POLLIN) {
        result = read(fd, buffer, count);
        fcntl(fd, F_SETFL, original_flags); // フラグを復元
        return result;
    }
    
    // その他のイベント
    fcntl(fd, F_SETFL, original_flags); // フラグを復元
    return -1;
}

typedef enum {
    ENCODE_MODE,
    DECODE_MODE
} process_mode_t;

void process_data_stream(int input_fd, int output_fd, process_mode_t mode, 
                        const config_t *config, FILE *log_file, const char *log_prefix,
                        const char *eof_message) {
    unsigned char input_buffer[BUFFER_SIZE];
    unsigned char output_buffer[MAX_ENCODED_SIZE];
    size_t buffer_pos = 0;
    ssize_t bytes_read, bytes_processed, bytes_written;
    size_t remaining_bytes = 0;
    
    while (1) {
        bytes_read = read_with_timeout(input_fd, input_buffer + buffer_pos, 
                                     BUFFER_SIZE - buffer_pos, 200);
        
        if (bytes_read < 0) {
            break;
        } else if (bytes_read == 0) {
            if (buffer_pos > 0) {
                if (mode == ENCODE_MODE) {
                    if (log_file) {
                        hex_dump_to_file(log_file, log_prefix, input_buffer, buffer_pos, config);
                    }
                    if (config->method == METHOD_UUENCODE) {
                        bytes_processed = uuencode_data(input_buffer, buffer_pos, output_buffer);
                    } else {
                        bytes_processed = escape_encode_data(input_buffer, buffer_pos, output_buffer);
                    }
                } else {
                    if (config->method == METHOD_UUENCODE) {
                        bytes_processed = uudecode_data(input_buffer, buffer_pos, output_buffer, &remaining_bytes);
                    } else {
                        bytes_processed = escape_decode_data(input_buffer, buffer_pos, output_buffer, &remaining_bytes);
                    }
                    if (log_file) {
                        hex_dump_to_file(log_file, log_prefix, input_buffer, buffer_pos - remaining_bytes, config);
                    }
                }

                if (log_file) {
                    const char *proc_prefix = (mode == ENCODE_MODE) ? "enc-d:" : "dec-d:";
                    hex_dump_to_file(log_file, proc_prefix, output_buffer, bytes_processed, config);
                }

                bytes_written = 0;
                while (bytes_written < bytes_processed) {
                    ssize_t written = write(output_fd, output_buffer + bytes_written,
                                          bytes_processed - bytes_written);
                    if (written <= 0) {
                        exit(1);
                    }
                    bytes_written += written;
                }
                
                // 残りバイトがある場合、バッファの先頭に移動
                if (remaining_bytes > 0) {
                    memmove(input_buffer, input_buffer + buffer_pos - remaining_bytes, remaining_bytes);
                    buffer_pos = remaining_bytes;
                } else {
                    buffer_pos = 0;
                }
            }
            continue;
        } else {
            buffer_pos += bytes_read;
            
            if (buffer_pos >= BUFFER_SIZE) {
                if (mode == ENCODE_MODE) {
                    if (log_file) {
                        hex_dump_to_file(log_file, log_prefix, input_buffer, buffer_pos, config);
                    }
                    if (config->method == METHOD_UUENCODE) {
                        bytes_processed = uuencode_data(input_buffer, buffer_pos, output_buffer);
                    } else {
                        bytes_processed = escape_encode_data(input_buffer, buffer_pos, output_buffer);
                    }
                } else {
                    if (config->method == METHOD_UUENCODE) {
                        bytes_processed = uudecode_data(input_buffer, buffer_pos, output_buffer, &remaining_bytes);
                    } else {
                        bytes_processed = escape_decode_data(input_buffer, buffer_pos, output_buffer, &remaining_bytes);
                    }
                    if (log_file) {
                        hex_dump_to_file(log_file, log_prefix, input_buffer, buffer_pos - remaining_bytes, config);
                    }
                }

                if (log_file) {
                    const char *proc_prefix = (mode == ENCODE_MODE) ? "enc-d:" : "dec-d:";
                    hex_dump_to_file(log_file, proc_prefix, output_buffer, bytes_processed, config);
                }

                bytes_written = 0;
                while (bytes_written < bytes_processed) {
                    ssize_t written = write(output_fd, output_buffer + bytes_written,
                                          bytes_processed - bytes_written);
                    if (written <= 0) {
                        exit(1);
                    }
                    bytes_written += written;
                }
                
                // 残りバイトがある場合、バッファの先頭に移動
                if (remaining_bytes > 0) {
                    memmove(input_buffer, input_buffer + buffer_pos - remaining_bytes, remaining_bytes);
                    buffer_pos = remaining_bytes;
                } else {
                    buffer_pos = 0;
                }
            }
        }
    }
    
    if (log_file) {
        struct timeval tv;
        struct tm *tm_info;
        gettimeofday(&tv, NULL);
        tm_info = localtime(&tv.tv_sec);

        fprintf(log_file, "%02d:%02d:%02d.%06d ", 
                tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, (int)tv.tv_usec);
        
        // ユーザー定義プレフィクスがあれば追加
        if (config && config->log_prefix) {
            fprintf(log_file, "%s:", config->log_prefix);
        }
        
        fprintf(log_file, "%s", eof_message);
        fflush(log_file);
        fclose(log_file);
    }
}

void handle_connection_common(int sockfd, int input_fd, int output_fd, const config_t *config) {
    pid_t pid1, pid2;

    pid1 = fork();
    if (pid1 == 0) {
        FILE *log_file = NULL;
        if (config->log_stdio_port_file) {
            log_file = fopen(config->log_stdio_port_file, "w");
        }
        
        process_data_stream(input_fd, sockfd, DECODE_MODE, config, log_file, 
                          "todec:", "from input: EOF detected");
        
        shutdown(sockfd, SHUT_WR);
        exit(0);
    } else if (pid1 > 0) {
        pid2 = fork();
        if (pid2 == 0) {
            FILE *log_file = NULL;
            if (config->log_port_stdio_file) {
                log_file = fopen(config->log_port_stdio_file, "w");
            }
            
            process_data_stream(sockfd, output_fd, ENCODE_MODE, config, log_file,
                              "toenc:", "from socket: EOF detected");
            
            exit(0);
        } else if (pid2 > 0) {
            int status;
            waitpid(pid1, &status, 0);
            waitpid(pid2, &status, 0);
        } else {
            perror("fork");
            kill(pid1, SIGTERM);
            waitpid(pid1, NULL, 0);
            exit(1);
        }
    } else {
        perror("fork");
        exit(1);
    }
}

void handle_connection(int sockfd, const config_t *config) {
    if (config->system_command) {
        int to_child_pipe[2];
        int from_child_pipe[2];

        if (pipe(to_child_pipe) == -1 || pipe(from_child_pipe) == -1) {
            perror("pipe");
            return;
        }

        pid_t cmd_pid = fork();

        if (cmd_pid < 0) {
            perror("fork for command");
            close(to_child_pipe[0]);
            close(to_child_pipe[1]);
            close(from_child_pipe[0]);
            close(from_child_pipe[1]);
            return;
        }

        if (cmd_pid == 0) { // Child process for the command
            close(to_child_pipe[1]);
            dup2(to_child_pipe[0], STDIN_FILENO);
            close(to_child_pipe[0]);

            close(from_child_pipe[0]);
            dup2(from_child_pipe[1], STDOUT_FILENO);
            close(from_child_pipe[1]);

            execl("/bin/sh", "sh", "-c", config->system_command, (char *)NULL);
            perror("execl");
            exit(1);
        }

        // Parent process
        close(to_child_pipe[0]);
        close(from_child_pipe[1]);
        int to_cmd_fd = to_child_pipe[1];
        int from_cmd_fd = from_child_pipe[0];

        handle_connection_common(sockfd, from_cmd_fd, to_cmd_fd, config);
        
        close(to_cmd_fd);
        close(from_cmd_fd);
        
        if (cmd_pid > 0) {
            int status;
            kill(cmd_pid, SIGTERM);
            waitpid(cmd_pid, &status, 0);
        }

    } else {
        // non-command mode
        handle_connection_common(sockfd, STDIN_FILENO, STDOUT_FILENO, config);
    }
}

int sender_mode(const config_t *config) {
    int client_sock;
    struct sockaddr_in server_addr;
    
    // ソケット作成
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("socket");
        return 1;
    }
    
    // サーバーアドレス設定
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config->port);
    
    if (inet_pton(AF_INET, config->host, &server_addr.sin_addr) <= 0) {
        if (!config->quiet) {
            fprintf(stderr, "Invalid address: %s\n", config->host);
        }
        close(client_sock);
        return 1;
    }
    
    // 接続
    if (!config->quiet) {
        fprintf(stderr, "Connecting to %s:%d...\n", config->host, config->port);
    }
    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(client_sock);
        return 1;
    }
    
    if (!config->quiet) {
        fprintf(stderr, "Connected to server\n");
    }
    
    // delayが設定されている場合は待機
    if (config->delay_seconds > 0) {
        if (!config->quiet) {
            fprintf(stderr, "Waiting %d seconds before handling connection...\n", config->delay_seconds);
        }
        sleep(config->delay_seconds);
    }
    
    // 接続処理
    handle_connection(client_sock, config);
    
    close(client_sock);
    if (!config->quiet) {
        fprintf(stderr, "Disconnected from server\n");
    }
    return 0;
}

int receiver_mode(const config_t *config) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int opt = 1;
    
    // ソケット作成
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return 1;
    }
    
    // ソケットオプション設定
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_sock);
        return 1;
    }
    
    // サーバーアドレス設定
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config->port);
    
    // バインド
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_sock);
        return 1;
    }
    
    // リッスン
    if (listen(server_sock, 1) < 0) {
        perror("listen");
        close(server_sock);
        return 1;
    }
    
    if (!config->quiet) {
        fprintf(stderr, "Waiting for connection on port %d...\n", config->port);
    }
    
    while (running) {
        // 接続待ち
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            if (running) {
                perror("accept");
            }
            break;
        }
        
        if (!config->quiet) {
            fprintf(stderr, "Client connected from %s:%d\n", 
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }
        
        // 新しいプロセスを作成して接続を処理
        pid_t connection_pid = fork();
        if (connection_pid == 0) {
            // 子プロセス: この接続を処理
            close(server_sock); // 子プロセスではサーバーソケットは不要
            
            // delayが設定されている場合は待機
            if (config->delay_seconds > 0) {
                if (!config->quiet) {
                    fprintf(stderr, "Waiting %d seconds before handling connection...\n", config->delay_seconds);
                }
                sleep(config->delay_seconds);
            }
            
            handle_connection(client_sock, config);
            close(client_sock);
            if (!config->quiet) {
                fprintf(stderr, "Client disconnected\n");
            }
            exit(0);
        } else if (connection_pid > 0) {
            // 親プロセス: 子プロセスは独立して動作するので、ソケットを閉じて次の接続を待つ
            close(client_sock);
        } else {
            perror("fork");
            close(client_sock);
        }
    }
    
    close(server_sock);
    return 0;
}
