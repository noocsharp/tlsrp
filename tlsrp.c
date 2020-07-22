#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <netdb.h>
#include <poll.h>
#include <tls.h>

#include "util.h"
#include "config.h"

#define BACKLOG 10
#define BUF_SIZE 1024
#define TIMEOUT 1000
#define SERVER 0
#define CLIENT 1

char *argv0;

void 
usage()
{
    fprintf(stderr, "usage: %s [-h backhost] [-H fronthost] -p backport -P frontport -ca ca_path -cert cert_path -key key_path\n", argv0);
    fprintf(stderr, "       %s -U unixsocket [-H fronthost] -P frontport -ca ca_path -cert cert_path -key key_path\n", argv0);
	exit(1);
}

static int
dobind(const char *host, const char *port)
{
    int sfd = -1;
    struct addrinfo *results = NULL, *rp = NULL;
    struct addrinfo hints = { .ai_family = AF_UNSPEC,
                              .ai_socktype = SOCK_STREAM};

    int err;
    if ((err = getaddrinfo(host, port, &hints, &results)) != 0)
        die("dobind: getaddrinfo: %s", gai_strerror(err));

    for (rp = results; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (sfd == -1)
            continue;

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(sfd);
    }
    
    if (rp == NULL)
        die("failed to bind:");

    free(results);
    return sfd;
}

static int
dounixconnect(const char *sockname)
{
    int sfd;
    struct sockaddr_un saddr = {0};

    if (!memccpy(saddr.sun_path, sockname, '\0', sizeof(saddr.sun_path)))
        die("unix socket path too long");

    saddr.sun_family = AF_UNIX;

    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        die("failed to create unix socket:");

    if (connect(sfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_un)) == -1) {
        close(sfd);
        die("failed to connect to unix socket:");
    }

    return sfd;
}

static int
donetworkconnect(const char* host, const char* port)
{
    int sfd = -1;
    struct addrinfo *results = NULL, *rp = NULL;
    struct addrinfo hints = { .ai_family = AF_UNSPEC,
                              .ai_socktype = SOCK_STREAM};

    if (getaddrinfo(host, port, &hints, &results) != 0)
        die("getaddrinfo failed:");

    for (rp = results; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(sfd);
    }
    
    if (rp == NULL)
        warn("failed to connect:");

    free(results);
    return sfd;
}

static int
serve(int serverfd, int clientfd, struct tls *clientconn)
{
    struct pollfd pfd[] = {
        {serverfd, POLLIN | POLLOUT, 0},
        {clientfd, POLLIN | POLLOUT, 0}
    };

    char clibuf[BUF_SIZE] = {0};
    char serbuf[BUF_SIZE] = {0};

    char *cliptr = NULL, *serptr = NULL;

    size_t clicount = 0, sercount = 0;
    ssize_t written = 0;

    while (poll(pfd, 2, TIMEOUT) != 0) {
        if ((pfd[CLIENT].revents | pfd[SERVER].revents) & POLLNVAL)
            return -1;

        if ((pfd[CLIENT].revents & POLLIN) && clicount == 0) {
            clicount = tls_read(clientconn, clibuf, BUF_SIZE);
            if (clicount == -1) {
                tdie("client read failed:");
                return -2;
            } else if (clicount == TLS_WANT_POLLIN) {
                pfd[CLIENT].events = POLLIN;
            } else if (clicount == TLS_WANT_POLLOUT) {
                pfd[CLIENT].events = POLLOUT;
            } else {
                cliptr = clibuf;
            }
        }

        if ((pfd[SERVER].revents & POLLIN) && sercount == 0) {
            sercount = read(serverfd, serbuf, BUF_SIZE);
            if (sercount == -1) {
                die("server read failed:");
                return -3;
            }
            serptr = serbuf;
        }

        if ((pfd[SERVER].revents & POLLOUT) && clicount > 0) {
            written = write(serverfd, cliptr, clicount);
            if (written == -1)
                die("failed to write:");
            clicount -= written;
            cliptr += written;
        }

        if ((pfd[CLIENT].revents & POLLOUT) && sercount > 0) {
            written = tls_write(clientconn, serptr, sercount);
            if (written == -1)
                tdie("failed tls_write:");
            else if (written == TLS_WANT_POLLIN) {
                pfd[CLIENT].events = POLLIN;
            } else if (written == TLS_WANT_POLLOUT) {
                pfd[CLIENT].events = POLLOUT;
            } else {
                sercount -= written;
                serptr += written;
            }
        }

        if ((pfd[CLIENT].revents | pfd[SERVER].revents) & POLLHUP)
            if (clicount == 0 && sercount == 0)
                break;

        if ((pfd[CLIENT].revents | pfd[SERVER].revents) & POLLERR)
            break;
    }
    return 0;
}

int 
main(int argc, char* argv[])
{
    int serverfd = 0, clientfd = 0, bindfd = 0;
    struct sockaddr_storage client_sa, server_sa = {0};
    struct tls_config *config;
    struct tls *tls_client, *conn;
    socklen_t client_sa_len = 0;
    char *usock = NULL,
         *backhost  = NULL,
         *fronthost  = NULL,
         *backport = NULL,
         *frontport = NULL,
         *ca_path = NULL,
         *cert_path = NULL,
         *key_path = NULL;

    argv0 = argv[0];

    if (argc < 3)
        usage();

    // TODO make parameter format enforcement stricter
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-U") == 0)
            usock = argv[++i];
        else if (strcmp(argv[i], "-h") == 0)
            backhost = argv[++i];
        else if (strcmp(argv[i], "-H") == 0)
            fronthost = argv[++i];
        else if (strcmp(argv[i], "-p") == 0)
            backport = argv[++i];
        else if (strcmp(argv[i], "-P") == 0)
            frontport = argv[++i];
        else if (strcmp(argv[i], "-ca") == 0)
            ca_path = argv[++i];
        else if (strcmp(argv[i], "-cert") == 0)
            cert_path = argv[++i];
        else if (strcmp(argv[i], "-key") == 0)
            key_path = argv[++i];
        else
            usage();
    }

    if (usock && (backhost || backport))
        die("cannot use both unix and network socket");

    if (!ca_path || !cert_path || !key_path)
        usage();

    if ((config = tls_config_new()) == NULL)
        tcdie("failed to get tls config:");

    if (tls_config_set_protocols(config, protocols) == -1)
        tcdie("failed to set protocols:");

    if (tls_config_set_ciphers(config, ciphers) == -1)
        tcdie("failed to set ciphers:");

    if (tls_config_set_dheparams(config, dheparams) == -1)
        tcdie("failed to set dheparams:");

    if (tls_config_set_ecdhecurves(config, ecdhecurves) == -1)
        tcdie("failed to set ecdhecurves:");

    if (tls_config_set_ca_file(config, ca_path) == -1)
        tcdie("failed to load ca file:");

    if (tls_config_set_cert_file(config, cert_path) == -1)
        tcdie("failed to load cert file:");

    if (tls_config_set_key_file(config, key_path) == -1)
        tcdie("failed to load key file:");

    if ((tls_client = tls_server()) == NULL)
        die("failed to create server context");

    if ((tls_configure(tls_client, config)) == -1)
        tdie("failed to configure server");
    
    tls_config_free(config);

    bindfd = dobind(fronthost, frontport);

    if (listen(bindfd, BACKLOG) == -1) {
        close(bindfd);
        die("could not start listen:");
    }

    pid_t pid;

    while (1) {
        if ((clientfd = accept(bindfd, (struct sockaddr*) &client_sa, 
                        &client_sa_len)) == -1) {
            warn("could not accept connection:");
        }

        switch ((pid = fork())) {
            case -1:
                warn("fork:");
            case 0:
                if (usock)
                    serverfd = dounixconnect(usock);
                else
                    serverfd = donetworkconnect(backhost, backport);

                if (tls_accept_socket(tls_client, &conn, clientfd) == -1) {
                    warn("tls_accept_socket: %s", tls_error(tls_client));
                    goto tlsfail;
                }

                if (serverfd)
                    serve(serverfd, clientfd, conn);

                tls_close(conn);
            tlsfail:
                close(serverfd);
                close(clientfd);
                close(bindfd);
                exit(0);
            default:
                close(clientfd);
        }
    }
}

