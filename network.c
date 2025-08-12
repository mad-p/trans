#include "trans.h"

void handle_connection(int sockfd, encode_method_t method, int is_sender) {
    (void)is_sender;
    pid_t pid1, pid2;
    unsigned char buffer[BUFFER_SIZE];
    char encoded_buffer[MAX_ENCODED_SIZE];
    unsigned char decoded_buffer[BUFFER_SIZE];
    
    pid1 = fork();
    if (pid1 == 0) {
        // 子プロセス1: 標準入力 -> エンコード -> ソケット
        ssize_t bytes_read, bytes_encoded, bytes_sent;
        
        while ((bytes_read = read(STDIN_FILENO, buffer, BUFFER_SIZE)) > 0) {
            if (method == METHOD_UUENCODE) {
                bytes_encoded = uuencode_data(buffer, bytes_read, encoded_buffer);
            } else {
                bytes_encoded = escape_encode_data(buffer, bytes_read, encoded_buffer);
            }
            
            bytes_sent = 0;
            while (bytes_sent < bytes_encoded) {
                ssize_t sent = write(sockfd, encoded_buffer + bytes_sent, 
                                   bytes_encoded - bytes_sent);
                if (sent <= 0) {
                    exit(1);
                }
                bytes_sent += sent;
            }
        }
        
        shutdown(sockfd, SHUT_WR);
        exit(0);
    } else if (pid1 > 0) {
        pid2 = fork();
        if (pid2 == 0) {
            // 子プロセス2: ソケット -> デコード -> 標準出力
            ssize_t bytes_received, bytes_decoded, bytes_written;
            
            while ((bytes_received = read(sockfd, encoded_buffer, MAX_ENCODED_SIZE - 1)) > 0) {
                encoded_buffer[bytes_received] = '\0';
                
                if (method == METHOD_UUENCODE) {
                    bytes_decoded = uudecode_data(encoded_buffer, bytes_received, decoded_buffer);
                } else {
                    bytes_decoded = escape_decode_data(encoded_buffer, bytes_received, decoded_buffer);
                }
                
                bytes_written = 0;
                while (bytes_written < bytes_decoded) {
                    ssize_t written = write(STDOUT_FILENO, decoded_buffer + bytes_written, 
                                          bytes_decoded - bytes_written);
                    if (written <= 0) {
                        exit(1);
                    }
                    bytes_written += written;
                }
            }
            
            exit(0);
        } else if (pid2 > 0) {
            // 親プロセス: 両方の子プロセスの終了を待つ
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

int sender_mode(const config_t *config) {
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
    
    fprintf(stderr, "Waiting for connection on port %d...\n", config->port);
    
    while (running) {
        // 接続待ち
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            if (running) {
                perror("accept");
            }
            break;
        }
        
        fprintf(stderr, "Client connected from %s:%d\n", 
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // 接続処理
        handle_connection(client_sock, config->method, 1);
        
        close(client_sock);
        fprintf(stderr, "Client disconnected\n");
        break; // 一つの接続のみ処理
    }
    
    close(server_sock);
    return 0;
}

int receiver_mode(const config_t *config) {
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
        fprintf(stderr, "Invalid address: %s\n", config->host);
        close(client_sock);
        return 1;
    }
    
    // 接続
    fprintf(stderr, "Connecting to %s:%d...\n", config->host, config->port);
    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(client_sock);
        return 1;
    }
    
    fprintf(stderr, "Connected to server\n");
    
    // 接続処理
    handle_connection(client_sock, config->method, 0);
    
    close(client_sock);
    fprintf(stderr, "Disconnected from server\n");
    return 0;
}