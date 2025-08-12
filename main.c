#include "trans.h"

volatile int running = 1;

void cleanup_and_exit(int sig) {
    (void)sig;
    running = 0;
    exit(0);
}

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s -m <send|recv> -p <port> [-h <host>] [-e <uuencode|escape>]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -m, --mode     Mode: send (sender) or recv (receiver)\n");
    fprintf(stderr, "  -p, --port     TCP port number\n");
    fprintf(stderr, "  -h, --host     Host (for receiver mode, default: localhost)\n");
    fprintf(stderr, "  -e, --encode   Encoding method: uuencode or escape (default: escape)\n");
    fprintf(stderr, "  --help         Show this help message\n");
}

void parse_arguments(int argc, char *argv[], config_t *config) {
    static struct option long_options[] = {
        {"mode", required_argument, 0, 'm'},
        {"port", required_argument, 0, 'p'},
        {"host", required_argument, 0, 'h'},
        {"encode", required_argument, 0, 'e'},
        {"help", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    config->mode = (trans_mode_t)-1;
    config->port = -1;
    config->method = METHOD_ESCAPE;
    config->host = "localhost";

    int c;
    int option_index = 0;

    while ((c = getopt_long(argc, argv, "m:p:h:e:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'm':
                if (strcmp(optarg, "send") == 0) {
                    config->mode = MODE_SENDER;
                } else if (strcmp(optarg, "recv") == 0) {
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