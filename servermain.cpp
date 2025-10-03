// server.cpp
// Simple TCP server that accepts one client, receives a message, and prints it.

#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

using namespace std;

int main(int argc, char **argv) {
    if (argc != 2) {
        cerr << "Usage: ./server <port>\n";
        return 1;
    }

    string port = argv[1];

    // ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    struct addrinfo hints{}, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP; // <--- add this
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(nullptr, port.c_str(), &hints, &res);
    if (rc != 0) {
        cerr << "getaddrinfo: " << gai_strerror(rc) << endl;
        return 1;
    }

    int listenfd = -1;
    for (auto rp = res; rp != nullptr; rp = rp->ai_next) {
        listenfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listenfd < 0) continue;
        int yes = 1;
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (bind(listenfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(listenfd);
        listenfd = -1;
    }
    freeaddrinfo(res);

    if (listenfd < 0) {
        cerr << "Failed to bind socket" << endl;
        return 1;
    }

    if (listen(listenfd, 5) != 0) {
        cerr << "listen failed" << endl;
        close(listenfd);
        return 1;
    }

    cout << "Server listening on port " << port << "..." << endl;

    struct sockaddr_storage cli_addr{};
    socklen_t cli_len = sizeof(cli_addr);
    int clientfd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_len);
    if (clientfd < 0) {
        cerr << "accept failed" << endl;
        close(listenfd);
        return 1;
    }

    char buf[1024];
    ssize_t n = recv(clientfd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        cout << "Server received: " << buf << endl;
    } else {
        cerr << "recv failed or connection closed" << endl;
    }

    close(clientfd);
    close(listenfd);
    return 0;
}
