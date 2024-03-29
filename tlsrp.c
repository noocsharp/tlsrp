#include <sys/socket.h>
#include <sys/un.h>

#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <tls.h>

#include "util.h"
#include "config.h"

#define BACKLOG 10
#define TIMEOUT 1000
#define SERVER 0
#define CLIENT 1

char *argv0;

static void
usage(void)
{
    fprintf(stderr, "usage: %s [-u backpath | -p backport [-h backhost]] [-U frontpath | -P frontport [-H fronthost]] -a ca_path -r cert_path -k key_path\n", argv0);
	exit(1);
}

static int
donetworkbind(const char *host, const char *port)
{
    int sfd;
    struct addrinfo *results, *rp;
    struct addrinfo hints = { .ai_family = AF_UNSPEC,
                              .ai_socktype = SOCK_STREAM};

    int err;
    if ((err = getaddrinfo(host, port, &hints, &results)) != 0)
        die("dobind: getaddrinfo: %s", gai_strerror(err));

    for (rp = results; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (sfd == -1)
            continue;

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            freeaddrinfo(results);
            return sfd;
        }

        close(sfd);
    }
    
    die("failed to bind:");
    return sfd;
}

static int
dounixbind(const char *path)
{
    struct sockaddr_un saddr = { .sun_family = AF_UNIX };
    int sfd;

    if (!memccpy(saddr.sun_path, path, '\0', sizeof(saddr.sun_path)))
        die("unix socket path too long:");

    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        die("failed to create unix-domain socket at %s:", path);

    if (bind(sfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_un)) == -1)
        die("failed to bind to socket at %s:", path);

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

    char clibuf[BUFSIZ] = {0};
    char serbuf[BUFSIZ] = {0};

    char *cliptr = NULL, *serptr = NULL;

    ssize_t clicount = 0, sercount = 0;
    ssize_t written = 0;

    while (poll(pfd, 2, TIMEOUT) != 0) {
        if ((pfd[CLIENT].revents | pfd[SERVER].revents) & POLLNVAL)
            return -1;

        if ((pfd[CLIENT].revents & POLLIN) && clicount == 0) {
            clicount = tls_read(clientconn, clibuf, BUFSIZ);
            if (clicount == -1) {
                tdie(clientconn, "client read failed:");
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
            sercount = read(serverfd, serbuf, BUFSIZ);
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
                tdie(clientconn, "failed tls_write:");
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
    struct sockaddr_storage client_sa = {0};
    struct tls_config *config;
    struct tls *toclient, *conn;
    socklen_t client_sa_len = 0;
    char *backpath = NULL,
         *frontpath = NULL,
         *backhost  = NULL,
         *fronthost  = NULL,
         *backport = NULL,
         *frontport = NULL,
         *ca_path = NULL,
         *cert_path = NULL,
         *key_path = NULL;
    int opt;
    char *optstring = "a:h:H:k:p:P:r:u:U:v";

    argv0 = argv[0];

    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {
            case 'a':
                ca_path = optarg;
                break;
            case 'h':
                backhost = optarg;
                break;
            case 'H':
                fronthost = optarg;
                break;
            case 'k':
                key_path = optarg;
                break;
            case 'p':
                backport = optarg;
                break;
            case 'P':
                frontport = optarg;
                break;
            case 'r':
                cert_path = optarg;
                break;
            case 'u':
                backpath = optarg;
                break;
            case 'U':
                frontpath = optarg;
                break;
            case 'v':
                printf("%s " VERSION "\n", argv0);
                exit(0);
                break;
            case '?':
            default:
                usage();
        }
    }

    if ((backpath && backhost) || !(backpath || backport))
        die("can only serve on unix socket xor network socket");

    if ((frontpath && fronthost) || !(frontpath || frontport))
        die("can only receive on unix socket xor network socket");

    if (!ca_path || !cert_path || !key_path)
        die("must provide ca_path, cert_path and key_path");

    if ((config = tls_config_new()) == NULL)
        tcdie(config, "failed to get tls config:");

    if (tls_config_set_protocols(config, protocols) == -1)
        tcdie(config, "failed to set protocols:");

    if (tls_config_set_ciphers(config, ciphers) == -1)
        tcdie(config, "failed to set ciphers:");

    if (tls_config_set_dheparams(config, dheparams) == -1)
        tcdie(config, "failed to set dheparams:");

    if (tls_config_set_ecdhecurves(config, ecdhecurves) == -1)
        tcdie(config, "failed to set ecdhecurves:");

    if (tls_config_set_ca_file(config, ca_path) == -1)
        tcdie(config, "failed to load ca file:");

    if (tls_config_set_cert_file(config, cert_path) == -1)
        tcdie(config, "failed to load cert file:");

    if (tls_config_set_key_file(config, key_path) == -1)
        tcdie(config, "failed to load key file:");

    if ((toclient = tls_server()) == NULL)
        die("failed to create server context");

    if ((tls_configure(toclient, config)) == -1)
        tdie(toclient, "failed to configure server:");
    
    tls_config_free(config);

    if (frontpath)
        bindfd = dounixbind(frontpath);
    else
        bindfd = donetworkbind(fronthost, frontport);


    if (listen(bindfd, BACKLOG) == -1) {
        close(bindfd);
        die("could not start listen:");
    }

    pid_t pid;

    while (1) {
        if ((clientfd = accept(bindfd, (struct sockaddr*) &client_sa, 
                        &client_sa_len)) == -1) {
            warn("could not accept connection:");
            continue;
        }

        switch ((pid = fork())) {
            case 0:
                if (backpath)
                    serverfd = dounixconnect(backpath);
                else
                    serverfd = donetworkconnect(backhost, backport);

                if (tls_accept_socket(toclient, &conn, clientfd) == -1) {
                    warn("tls_accept_socket: %s", tls_error(toclient));
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
            case -1:
                warn("fork:");
            default:
                close(clientfd);
        }
    }
}

