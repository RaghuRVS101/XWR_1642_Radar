// clientmain.cpp
// Client that sends the contents of a file to the server
// Build with: g++ -std=c++17 -Wall -O2 -o client clientmain.cpp

#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: ./client <host:port> <file>\n";
        return 1;
    }

    string arg = argv[1];
    string filename = argv[2];
    string host, port;

    size_t pos = arg.rfind(':');
    if (pos == string::npos) {
        cerr << "Bad address format. Use host:port\n";
        return 1;
    }
    host = arg.substr(0, pos);
    port = arg.substr(pos + 1);

    // Open file
    ifstream infile(filename, ios::binary);
    if (!infile) {
        cerr << "Could not open file: " << filename << "\n";
        return 1;
    }

    // Resolve address
    struct addrinfo hints{}, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0) {
        cerr << "getaddrinfo: " << gai_strerror(rc) << "\n";
        return 1;
    }

    int sockfd = -1;
    struct addrinfo *rp;
    for (rp = res; rp != nullptr; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sockfd);
        sockfd = -1;
    }
    if (sockfd == -1) {
        cerr << "Could not connect to server\n";
        freeaddrinfo(res);
        return 1;
    }
    freeaddrinfo(res);

    cout << "Connected to server. Sending file: " << filename << endl;

    // Send file contents
    char buffer[1024];
    while (infile.read(buffer, sizeof(buffer)) || infile.gcount() > 0) {
        ssize_t bytes = send(sockfd, buffer, infile.gcount(), 0);
        if (bytes == -1) {
            cerr << "Error sending file data\n";
            break;
        }
    }

    cout << "File sent successfully.\n";

    infile.close();
    close(sockfd);
    return 0;
}
