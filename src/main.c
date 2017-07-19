#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <ev.h>

#include "logger.h"
#include "netutils.h"
#include "resolve.h"
#include "callback.h"
#include "socks5.h"

#define EPOLL_SIZE 1024
#define LISTEN_BACKLOG 128
#define MAX_EVENTS_COUNT 128
#define LISTEN_PORT 23456
#define SERVER_DEFAULT_USERNAME "uusername"
#define SERVER_DEFAULT_PASSWORD "ppassword"

static void signal_handler(int sig) {
    logger_info("receive signal: [%d]\n", sig);

    switch (sig) {
        case SIGINT:
        case SIGTERM:
            break;
        default:
            logger_warn("unkown signal [%d]\n", sig);
    }
}

static int register_signals() {
    // ctrl + c
    if (SIG_ERR == signal(SIGINT, signal_handler)) {
        logger_error("register signal SIGINT fail\n");
        return -1;
    }

    // pkill
    if (SIG_ERR == signal(SIGTERM, signal_handler)) {
        logger_error("register signal SIGTERM fail\n");
        return -1;
    }

    return 0;
}

int create_and_bind(uint16_t port, int32_t backlog) {
    int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0) {
        logger_error("sockfd create failed");
        return -1;
    }

    struct sockaddr_in6 servaddr;
    memset((char *)&servaddr, 0, sizeof(servaddr));
    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_addr = in6addr_any;
    servaddr.sin6_port = htons(port);

    if (set_nonblocking(sockfd) < 0) {
        logger_error("set_nonblocking fail [%d]\n", errno);
        close(sockfd);
        return -1;
    }

    if (set_reuseaddr(sockfd) < 0) {
        logger_error("set_reuseaddr fail [%d]\n", errno);
        close(sockfd);
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        logger_error("bind error [%d]\n", errno);
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, backlog) < 0) {
        logger_error("listen error [%d]!\n", errno);
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int main (int argc, char **argv) {
    logger_info("starting ...\n");

    struct ev_loop *loop = ev_default_loop(0);
    struct ev_io server_watcher;
    struct socks5_server server = {
        strlen(SERVER_DEFAULT_USERNAME),
        SERVER_DEFAULT_USERNAME,
        strlen(SERVER_DEFAULT_PASSWORD),
        SERVER_DEFAULT_PASSWORD,
        LISTEN_PORT
    };

    if (resolve_init(loop, NULL, 0) < 0) {
        logger_error("resolve_init fail\n");
        exit(EXIT_FAILURE);
    } else {
        logger_info("resolve_init ok\n");
    }

    // if (register_signals() < 0) {
    //     logger_error("register_signals fail, errno: [%d]\n", errno);
    //     exit(EXIT_FAILURE);
    // }

    server_watcher.fd = create_and_bind(server.port, LISTEN_BACKLOG);
    server_watcher.data = &server;
    if (server_watcher.fd < 0) {
        logger_error("create_and_bind fail, errno: [%d]\n", errno);
        exit(EXIT_FAILURE);
    }

    ev_io_init(&server_watcher, accept_cb, server_watcher.fd, EV_READ);
    ev_io_start(loop, &server_watcher);

    ev_run(loop, 0);

    logger_info("exiting ...\n");

    resolve_shutdown(loop);
    ev_loop_destroy(loop);

    return EXIT_SUCCESS;
}
