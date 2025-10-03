// servermain.cpp
// Multi-client TCP server that prints messages from clients
// Build with: g++ -std=c++17 -Wall -O2 -o server servermain.cpp

#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << "Usage: ./server <ip:port>\n";
        return 1;
    }

    string arg = argv[1];
    string host, port;

    // Parse ip:port
    size_t pos = arg.rfind(':');
    if (pos == string::npos) {
        cerr << "Bad address format. Use ip:port\n";
        return 1;
    }
    host = arg.substr(0, pos);
    port = arg.substr(pos + 1);

    // Prepare addrinfo
    struct addrinfo hints{}, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0) {
        cerr << "getaddrinfo: " << gai_strerror(rc) << "\n";
        return 1;
    }

    int listenfd = -1;
    for (rp = res; rp != nullptr; rp = rp->ai_next) {
        listenfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listenfd == -1) continue;

        int yes = 1;
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(listenfd, rp->ai_addr, rp->ai_addrlen) == 0) break;

        close(listenfd);
        listenfd = -1;
    }
    freeaddrinfo(res);

    if (listenfd == -1) {
        cerr << "Failed to bind\n";
        return 1;
    }

    if (listen(listenfd, 10) == -1) {
        perror("listen");
        close(listenfd);
        return 1;
    }

    cout << "Server listening on " << host << ":" << port << " ...\n";

    // Avoid zombie processes
    signal(SIGCHLD, SIG_IGN);

    while (true) {
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int clientfd = accept(listenfd, (struct sockaddr *)&client_addr, &addr_size);
        if (clientfd == -1) {
            perror("accept");
            continue;
        }

        if (fork() == 0) {
            close(listenfd); // child does not need listener

            char ipstr[INET6_ADDRSTRLEN];
            void *addrptr;
            if (client_addr.ss_family == AF_INET) {
                struct sockaddr_in *s = (struct sockaddr_in*)&client_addr;
                addrptr = &s->sin_addr;
            } else {
                struct sockaddr_in6 *s = (struct sockaddr_in6*)&client_addr;
                addrptr = &s->sin6_addr;
            }
            inet_ntop(client_addr.ss_family, addrptr, ipstr, sizeof(ipstr));
            cout << "Client connected from " << ipstr << endl;

            // Receive data
            char buffer[1024];
            ssize_t n;
            while ((n = recv(clientfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
                buffer[n] = '\0';
                cout << "Message from " << ipstr << ": " << buffer << endl;
            }

            close(clientfd);
            exit(0);
        }

        close(clientfd); // parent closes, child handles client
    }

    close(listenfd);
    return 0;
}
