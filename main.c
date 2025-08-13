#include "trans.h"

volatile int running = 1;

void cleanup_and_exit(int sig) {
    (void)sig;
    running = 0;
    exit(0);
}

void hex_dump_to_file(FILE *file, const char *prefix, const unsigned char *data, size_t len, const config_t *config) {
    if (!file || !data || len == 0) return;
    
    struct timeval tv;
    struct tm *tm_info;
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    fprintf(file, "%02d:%02d:%02d.%06d ", 
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, (int)tv.tv_usec);
    
    // ユーザー定義プレフィクスがあれば追加
    if (config && config->log_prefix) {
        fprintf(file, "%s:", config->log_prefix);
    }
    
    fprintf(file, "%s", prefix);
    
    for (size_t i = 0; i < len; i++) {
        fprintf(file, "%02x", data[i]);
        if (i < len - 1) {
            fprintf(file, " ");
        }
    }
    fprintf(file, "\n");
    fflush(file);
}

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s -m <send|recv|to|from> -p <port> [options]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -m, --mode             Mode: send/to (connector) or recv/from (listener)\n");
    fprintf(stderr, "  -p, --port             TCP port number\n");
    fprintf(stderr, "  -h, --host             Host (for sender mode, default: 127.0.0.1)\n");
    fprintf(stderr, "  -e, --encode           Encoding method: uuencode or escape (default: escape)\n");
    fprintf(stderr, "  -s, --system           Connect to command instead of stdio\n");
    fprintf(stderr, "  -q, --quiet            Suppress stderr output\n");
    fprintf(stderr, "      --lps, --log-port-stdio  Log port->stdio/command traffic (hex dump)\n");
    fprintf(stderr, "      --lsp, --log-stdio-port  Log stdio/command->port traffic (hex dump)\n");
    fprintf(stderr, "      --log-prefix       Custom prefix for log entries (default: none)\n");
    fprintf(stderr, "      --ll               Alias for --log-prefix l --lps log_lps.log --lsp log_lsp.log\n");
    fprintf(stderr, "      --lr               Alias for --log-prefix r --lps log_rps.log --lsp log_rsp.log\n");
    fprintf(stderr, "      --version          Show version information\n");
    fprintf(stderr, "      --help             Show this help message\n");
}

void parse_arguments(int argc, char *argv[], config_t *config) {
    static struct option long_options[] = {
        {"mode", required_argument, 0, 'm'},
        {"port", required_argument, 0, 'p'},
        {"host", required_argument, 0, 'h'},
        {"encode", required_argument, 0, 'e'},
        {"system", required_argument, 0, 's'},
        {"quiet", no_argument, 0, 'q'},
        {"log-port-stdio", required_argument, 0, 1000},
        {"lps", required_argument, 0, 1000},
        {"log-stdio-port", required_argument, 0, 1001},
        {"lsp", required_argument, 0, 1001},
        {"log-prefix", required_argument, 0, 1002},
        {"ll", no_argument, 0, 1004},
        {"lr", no_argument, 0, 1005},
        {"version", no_argument, 0, 1003},
        {"help", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    config->mode = (trans_mode_t)-1;
    config->port = -1;
    config->method = METHOD_ESCAPE;
    config->host = "127.0.0.1";
    config->system_command = NULL;
    config->quiet = 0;
    config->log_port_stdio_file = NULL;
    config->log_stdio_port_file = NULL;
    config->log_prefix = NULL;

    int c;
    int option_index = 0;

    while ((c = getopt_long(argc, argv, "m:p:h:e:s:q", long_options, &option_index)) != -1) {
        switch (c) {
            case 'm':
                if (strcmp(optarg, "send") == 0 || strcmp(optarg, "to") == 0) {
                    config->mode = MODE_SENDER;
                } else if (strcmp(optarg, "recv") == 0 || strcmp(optarg, "from") == 0) {
                    config->mode = MODE_RECEIVER;
                } else {
                    fprintf(stderr, "Error: Invalid mode '%s'\n", optarg);
                    exit(1);
                }
                break;
            case 'p':
                config->port = atoi(optarg);
                if (config->port <= 0 || config->port > 65535) {
                    fprintf(stderr, "Error: Invalid port number '%s'\n", optarg);
                    exit(1);
                }
                break;
            case 'h':
                config->host = optarg;
                break;
            case 'e':
                if (strcmp(optarg, "uuencode") == 0) {
                    config->method = METHOD_UUENCODE;
                } else if (strcmp(optarg, "escape") == 0) {
                    config->method = METHOD_ESCAPE;
                } else {
                    fprintf(stderr, "Error: Invalid encoding method '%s'\n", optarg);
                    exit(1);
                }
                break;
            case 's':
                config->system_command = optarg;
                break;
            case 'q':
                config->quiet = 1;
                break;
            case 1000:
                config->log_port_stdio_file = optarg;
                break;
            case 1001:
                config->log_stdio_port_file = optarg;
                break;
            case 1002:
                config->log_prefix = optarg;
                break;
            case 1004: // --ll
                config->log_prefix = "l";
                config->log_port_stdio_file = "log_lps.log";
                config->log_stdio_port_file = "log_lsp.log";
                break;
            case 1005: // --lr
                config->log_prefix = "r";
                config->log_port_stdio_file = "log_rps.log";
                config->log_stdio_port_file = "log_rsp.log";
                break;
            case 1003:
                printf("trans version %s\n", TRANS_VERSION);
                exit(0);
            case 0:
                if (strcmp(long_options[option_index].name, "help") == 0) {
                    print_usage(argv[0]);
                    exit(0);
                }
                break;
            case '?':
                exit(1);
        }
    }


    if (config->mode == (trans_mode_t)-1 || config->port == -1) {
        fprintf(stderr, "Error: Mode and port are required\n");
        print_usage(argv[0]);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    config_t config;
    
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    signal(SIGPIPE, SIG_IGN);

    parse_arguments(argc, argv, &config);

    if (config.mode == MODE_SENDER) {
        return sender_mode(&config);
    } else {
        return receiver_mode(&config);
    }
}
