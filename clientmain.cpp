// client.cpp
// Simple TCP client that connects to server and sends a message.

#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
using namespace std;

int main(int argc, char **argv) {
    if (argc != 3) {
        cerr << "Usage: ./client <host> <port>\n";
        return 1;
    }

    string host = argv[1];
    string port = argv[2];

    struct addrinfo hints{}, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0) {
        cerr << "getaddrinfo: " << gai_strerror(rc) << endl;
        return 1;
    }

    int sock = -1;
    for (auto rp = res; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);

    if (sock < 0) {
        cerr << "Could not connect" << endl;
        return 1;
    }

    string msg = "Hello Server from Client!\n";
    send(sock, msg.c_str(), msg.size(), 0);
    cout << "Client sent: " << msg;

    close(sock);
    return 0;
}
