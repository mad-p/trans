#include "trans.h"

ssize_t read_with_timeout(int fd, void *buffer, size_t count, int timeout_ms) {
    fd_set read_fds;
    struct timeval timeout;
    int select_result;
    
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    select_result = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    
    if (select_result < 0) {
        return -1;
    } else if (select_result == 0) {
        return 0;
    } else if (FD_ISSET(fd, &read_fds)) {
        return read(fd, buffer, count);
    }
    
    return -1;
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

        pid_t pid1, pid2;

        pid1 = fork(); // Process 1: command stdout -> decode -> socket
        if (pid1 == 0) {
            close(to_cmd_fd);
            unsigned char decoded_buffer[MAX_ENCODED_SIZE];
            ssize_t bytes_read, bytes_decoded, bytes_sent;
            FILE *log_file = NULL;

            if (config->log_stdio_port_file) {
                log_file = fopen(config->log_stdio_port_file, "w");
            }

            unsigned char cmd_buffer[BUFFER_SIZE];
            size_t cmd_buffer_pos = 0;
            
            while (1) {
                bytes_read = read_with_timeout(from_cmd_fd, cmd_buffer + cmd_buffer_pos, 
                                             BUFFER_SIZE - cmd_buffer_pos, 200);
                
                if (bytes_read < 0) {
                    break;
                } else if (bytes_read == 0) {
                    if (cmd_buffer_pos > 0) {
                        if (log_file) {
                            hex_dump_to_file(log_file, "todec:", cmd_buffer, cmd_buffer_pos);
                        }

                        if (config->method == METHOD_UUENCODE) {
                            bytes_decoded = uudecode_data(cmd_buffer, cmd_buffer_pos, decoded_buffer);
                        } else {
                            bytes_decoded = escape_decode_data(cmd_buffer, cmd_buffer_pos, decoded_buffer);
                        }

                        if (log_file) {
                            hex_dump_to_file(log_file, "dec-d:", decoded_buffer, bytes_decoded);
                        }

                        bytes_sent = 0;
                        while (bytes_sent < bytes_decoded) {
                            ssize_t sent = write(sockfd, decoded_buffer + bytes_sent, bytes_decoded - bytes_sent);
                            if (sent <= 0) {
                                exit(1);
                            }
                            bytes_sent += sent;
                        }
                        cmd_buffer_pos = 0;
                    }
                    continue;
                } else {
                    cmd_buffer_pos += bytes_read;
                    
                    if (cmd_buffer_pos >= BUFFER_SIZE) {
                        if (log_file) {
                            hex_dump_to_file(log_file, "todec:", cmd_buffer, cmd_buffer_pos);
                        }

                        if (config->method == METHOD_UUENCODE) {
                            bytes_decoded = uudecode_data(cmd_buffer, cmd_buffer_pos, decoded_buffer);
                        } else {
                            bytes_decoded = escape_decode_data(cmd_buffer, cmd_buffer_pos, decoded_buffer);
                        }

                        if (log_file) {
                            hex_dump_to_file(log_file, "dec-d:", decoded_buffer, bytes_decoded);
                        }

                        bytes_sent = 0;
                        while (bytes_sent < bytes_decoded) {
                            ssize_t sent = write(sockfd, decoded_buffer + bytes_sent, bytes_decoded - bytes_sent);
                            if (sent <= 0) {
                                exit(1);
                            }
                            bytes_sent += sent;
                        }
                        cmd_buffer_pos = 0;
                    }
                }
            }
            
            if (log_file) {
                fprintf(log_file, "EOF detected from command stdout\n");
                fflush(log_file);
                fclose(log_file);
            }
            
            close(from_cmd_fd);
            shutdown(sockfd, SHUT_WR);
            exit(0);
        }

        pid2 = fork(); // Process 2: socket -> encode -> command stdin
        if (pid2 == 0) {
            close(from_cmd_fd);
            unsigned char encoded_buffer[BUFFER_SIZE];
            ssize_t bytes_received, bytes_encoded, bytes_written;
            FILE *log_file = NULL;

            if (config->log_port_stdio_file) {
                log_file = fopen(config->log_port_stdio_file, "w");
            }

            unsigned char socket_buffer[BUFFER_SIZE];
            size_t socket_buffer_pos = 0;
            
            while (1) {
                bytes_received = read_with_timeout(sockfd, socket_buffer + socket_buffer_pos, 
                                                 BUFFER_SIZE - socket_buffer_pos, 200);
                
                if (bytes_received < 0) {
                    break;
                } else if (bytes_received == 0) {
                    if (socket_buffer_pos > 0) {
                        if (log_file) {
                            hex_dump_to_file(log_file, "toenc:", socket_buffer, socket_buffer_pos);
                        }

                        if (config->method == METHOD_UUENCODE) {
                            bytes_encoded = uuencode_data(socket_buffer, socket_buffer_pos, encoded_buffer);
                        } else {
                            bytes_encoded = escape_encode_data(socket_buffer, socket_buffer_pos, encoded_buffer);
                        }

                        if (log_file) {
                            hex_dump_to_file(log_file, "enc-d:", (unsigned char*)encoded_buffer, bytes_encoded);
                        }

                        bytes_written = 0;
                        while (bytes_written < bytes_encoded) {
                            ssize_t written = write(to_cmd_fd, encoded_buffer + bytes_written, bytes_encoded - bytes_written);
                            if (written <= 0) {
                                exit(1);
                            }
                            bytes_written += written;
                        }
                        socket_buffer_pos = 0;
                    }
                    continue;
                } else {
                    socket_buffer_pos += bytes_received;
                    
                    if (socket_buffer_pos >= BUFFER_SIZE) {
                        if (log_file) {
                            hex_dump_to_file(log_file, "toenc:", socket_buffer, socket_buffer_pos);
                        }

                        if (config->method == METHOD_UUENCODE) {
                            bytes_encoded = uuencode_data(socket_buffer, socket_buffer_pos, encoded_buffer);
                        } else {
                            bytes_encoded = escape_encode_data(socket_buffer, socket_buffer_pos, encoded_buffer);
                        }

                        if (log_file) {
                            hex_dump_to_file(log_file, "enc-d:", (unsigned char*)encoded_buffer, bytes_encoded);
                        }

                        bytes_written = 0;
                        while (bytes_written < bytes_encoded) {
                            ssize_t written = write(to_cmd_fd, encoded_buffer + bytes_written, bytes_encoded - bytes_written);
                            if (written <= 0) {
                                exit(1);
                            }
                            bytes_written += written;
                        }
                        socket_buffer_pos = 0;
                    }
                }
            }
            
            if (log_file) {
                fprintf(log_file, "EOF detected from socket\n");
                fflush(log_file);
                fclose(log_file);
            }
            
            close(to_cmd_fd);
            exit(0);
        }

        // Parent waits for all children
        int status;
        if (pid1 > 0) {
            waitpid(pid1, &status, 0);
        }
        // Once the reading from the command is done, we can close the writing pipe to it.
        close(to_cmd_fd);

        if (pid2 > 0) {
            waitpid(pid2, &status, 0);
        }
        
        if (cmd_pid > 0) {
             kill(cmd_pid, SIGTERM);
             waitpid(cmd_pid, &status, 0);
        }
        close(from_cmd_fd);

    } else {
        // non-command mode
        pid_t pid1, pid2;
        unsigned char encoded_buffer[MAX_ENCODED_SIZE];
        unsigned char decoded_buffer[BUFFER_SIZE];

        pid1 = fork();
        if (pid1 == 0) {
            // 子プロセス1: 標準入力 -> デコード -> ソケット
            ssize_t bytes_read, bytes_decoded, bytes_sent;
            FILE *log_file = NULL;
            unsigned char input_buffer[BUFFER_SIZE];
            size_t buffer_pos = 0;

            if (config->log_stdio_port_file) {
                log_file = fopen(config->log_stdio_port_file, "w");
            }

            while (1) {
                bytes_read = read_with_timeout(STDIN_FILENO, input_buffer + buffer_pos, 
                                             BUFFER_SIZE - buffer_pos, 200);
                
                if (bytes_read < 0) {
                    break;
                } else if (bytes_read == 0) {
                    if (buffer_pos > 0) {
                        if (log_file) {
                            hex_dump_to_file(log_file, "todec:", input_buffer, buffer_pos);
                        }

                        if (config->method == METHOD_UUENCODE) {
                            bytes_decoded = uudecode_data(input_buffer, buffer_pos, decoded_buffer);
                        } else {
                            bytes_decoded = escape_decode_data(input_buffer, buffer_pos, decoded_buffer);
                        }

                        if (log_file) {
                            hex_dump_to_file(log_file, "dec-d:", decoded_buffer, bytes_decoded);
                        }

                        bytes_sent = 0;
                        while (bytes_sent < bytes_decoded) {
                            ssize_t sent = write(sockfd, decoded_buffer + bytes_sent,
                                               bytes_decoded - bytes_sent);
                            if (sent <= 0) {
                                exit(1);
                            }
                            bytes_sent += sent;
                        }
                        buffer_pos = 0;
                    }
                    continue;
                } else {
                    buffer_pos += bytes_read;
                    
                    if (buffer_pos >= BUFFER_SIZE) {
                        if (log_file) {
                            hex_dump_to_file(log_file, "todec:", input_buffer, buffer_pos);
                        }

                        if (config->method == METHOD_UUENCODE) {
                            bytes_decoded = uudecode_data(input_buffer, buffer_pos, decoded_buffer);
                        } else {
                            bytes_decoded = escape_decode_data(input_buffer, buffer_pos, decoded_buffer);
                        }

                        if (log_file) {
                            hex_dump_to_file(log_file, "dec-d:", decoded_buffer, bytes_decoded);
                        }

                        bytes_sent = 0;
                        while (bytes_sent < bytes_decoded) {
                            ssize_t sent = write(sockfd, decoded_buffer + bytes_sent,
                                               bytes_decoded - bytes_sent);
                            if (sent <= 0) {
                                exit(1);
                            }
                            bytes_sent += sent;
                        }
                        buffer_pos = 0;
                    }
                }
            }

            if (log_file) {
                fprintf(log_file, "EOF detected from stdin\n");
                fflush(log_file);
                fclose(log_file);
            }

            shutdown(sockfd, SHUT_WR);
            exit(0);
        } else if (pid1 > 0) {
            pid2 = fork();
            if (pid2 == 0) {
                // 子プロセス2: ソケット -> エンコード -> 標準出力
                ssize_t bytes_received, bytes_encoded, bytes_written;
                FILE *log_file = NULL;

                if (config->log_port_stdio_file) {
                    log_file = fopen(config->log_port_stdio_file, "w");
                }

                unsigned char socket_out_buffer[BUFFER_SIZE];
                size_t socket_out_buffer_pos = 0;
                
                while (1) {
                    bytes_received = read_with_timeout(sockfd, socket_out_buffer + socket_out_buffer_pos, 
                                                     BUFFER_SIZE - socket_out_buffer_pos, 200);
                    
                    if (bytes_received < 0) {
                        break;
                    } else if (bytes_received == 0) {
                        if (socket_out_buffer_pos > 0) {
                            if (log_file) {
                                hex_dump_to_file(log_file, "toenc:", socket_out_buffer, socket_out_buffer_pos);
                            }

                            if (config->method == METHOD_UUENCODE) {
                                bytes_encoded = uuencode_data(socket_out_buffer, socket_out_buffer_pos, encoded_buffer);
                            } else {
                                bytes_encoded = escape_encode_data(socket_out_buffer, socket_out_buffer_pos, encoded_buffer);
                            }

                            if (log_file) {
                                hex_dump_to_file(log_file, "enc-d:", (unsigned char*)encoded_buffer, bytes_encoded);
                            }

                            bytes_written = 0;
                            while (bytes_written < bytes_encoded) {
                                ssize_t written = write(STDOUT_FILENO, encoded_buffer + bytes_written,
                                                      bytes_encoded - bytes_written);
                                if (written <= 0) {
                                    exit(1);
                                }
                                bytes_written += written;
                            }
                            socket_out_buffer_pos = 0;
                        }
                        continue;
                    } else {
                        socket_out_buffer_pos += bytes_received;
                        
                        if (socket_out_buffer_pos >= BUFFER_SIZE) {
                            if (log_file) {
                                hex_dump_to_file(log_file, "toenc:", socket_out_buffer, socket_out_buffer_pos);
                            }

                            if (config->method == METHOD_UUENCODE) {
                                bytes_encoded = uuencode_data(socket_out_buffer, socket_out_buffer_pos, encoded_buffer);
                            } else {
                                bytes_encoded = escape_encode_data(socket_out_buffer, socket_out_buffer_pos, encoded_buffer);
                            }

                            if (log_file) {
                                hex_dump_to_file(log_file, "enc-d:", (unsigned char*)encoded_buffer, bytes_encoded);
                            }

                            bytes_written = 0;
                            while (bytes_written < bytes_encoded) {
                                ssize_t written = write(STDOUT_FILENO, encoded_buffer + bytes_written,
                                                      bytes_encoded - bytes_written);
                                if (written <= 0) {
                                    exit(1);
                                }
                                bytes_written += written;
                            }
                            socket_out_buffer_pos = 0;
                        }
                    }
                }

                if (log_file) {
                    fprintf(log_file, "EOF detected from socket\n");
                    fflush(log_file);
                    fclose(log_file);
                }

                exit(0);
            } else if (pid2 > 0) {
                // 親プロセス: 子プロセスの適切な終了処理
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
