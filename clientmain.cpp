#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
using namespace std;


static void print_stdout(const string &s) {
    // All protocol messages and errors must go to STDOUT
    cout << s << flush;
}

static string readline(int fd) {
    string out;
    char c;
    while (true) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) return string(); // connection closed or error => return empty
        out.push_back(c);
        if (c == '\n') break;
    }
    return out;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        print_stdout("Usage: ./client <host:port>\n");
        return 1;
    }
    string arg = argv[1];
    string host, port;
    // parse [ipv6]:port or host:port
    if (!arg.empty() && arg.front() == '[') {
        // [ipv6]:port
        auto pos = arg.find("]:");
        if (pos == string::npos) {
            print_stdout("ERROR: RESOLVE ISSUE\n");
            return 1;
        }
        host = arg.substr(1, pos-1);
        port = arg.substr(pos+2);
    } else {
        auto pos = arg.rfind(':');
        if (pos == string::npos) {
            print_stdout("ERROR: RESOLVE ISSUE\n");
            return 1;
        }
        host = arg.substr(0, pos);
        port = arg.substr(pos+1);
    }
    cout << "Host " << host << ", and port " << port << "." << endl;

    // Resolve
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC; // IPv4 + IPv6
    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0) {
        print_stdout("ERROR: RESOLVE ISSUE\n");
        return 1;
    }

    int sock = -1;
    struct addrinfo *rp;
    char ipstr[INET6_ADDRSTRLEN];
    for (rp = res; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            // connected
            void *addrptr = nullptr;
            if (rp->ai_family == AF_INET) {
                struct sockaddr_in *s = (struct sockaddr_in*)rp->ai_addr;
                addrptr = &s->sin_addr;
            } else {
                struct sockaddr_in6 *s = (struct sockaddr_in6*)rp->ai_addr;
                addrptr = &s->sin6_addr;
            }
            inet_ntop(rp->ai_family, addrptr, ipstr, sizeof(ipstr));
            break;
        }
        close(sock);
        sock = -1;
    }
    if (sock < 0) {
        // Could not connect to any resolved address. Use first resolved address ip for message if possible
        if (res) {
            char tmp[INET6_ADDRSTRLEN] = {'\0'};
            void *addrptr = nullptr;
            if (res->ai_family == AF_INET) {
                struct sockaddr_in *s = (struct sockaddr_in*)res->ai_addr;
                addrptr = &s->sin_addr;
            } else {
                struct sockaddr_in6 *s = (struct sockaddr_in6*)res->ai_addr;
                addrptr = &s->sin6_addr;
            }
            if (addrptr) inet_ntop(res->ai_family, addrptr, tmp, sizeof(tmp));
            string ipmsg = tmp;
            if (ipmsg.empty()) ipmsg = host;
            print_stdout(string("ERROR: CANT CONNECT TO ") + ipmsg + "\n");
        } else {
            print_stdout("ERROR: CANT CONNECT TO " + host + "\n");
        }
        freeaddrinfo(res);
        return 1;
    }
    freeaddrinfo(res);

    // --- Robust protocol banner read: read until blank line, then decide ---
    {
        bool validProtocol = false;
        std::string accumulated;
        char b[1024];

        // Keep reading until we see the blank line that ends the banner.
        while (true) {
            ssize_t n = recv(sock, b, sizeof(b), 0);
            if (n <= 0) break;
            accumulated.append(b, b + n);
            // Stop if we see an empty line (handles LF or CRLF)
            if (accumulated.find("\n\n") != string::npos ||
                accumulated.find("\r\n\r\n") != string::npos) {
                break;
            }
            // Safety: if banner is strangely long, stop anyway
            if (accumulated.size() > 8192) break;
        }

        // Scan lines for "TEXT TCP ..."
        {
            std::stringstream ss(accumulated);
            std::string l;
            while (std::getline(ss, l)) {
                if (!l.empty() && l.back() == '\r') l.pop_back();
                if (l.empty()) break; // blank line = end of versions
                if (l.rfind("TEXT TCP", 0) == 0) validProtocol = true;
            }
        }

        if (!validProtocol) {
            // For non-speaking servers (e.g., port 19), spec expects plain "ERROR"
            std::cout << "ERROR" << std::endl;
            close(sock);
            return 0;
        }

        // Acknowledge protocol
        const char okmsg[] = "OK\n";
        send(sock, okmsg, strlen(okmsg), 0);
    }

    // read assignment line
    string assign = readline(sock);
    if (assign.empty()) {
        close(sock);
        print_stdout("ERROR: RESOLVE ISSUE\n");
        return 1;
    }
    // Trim newline + optional CR
    if (!assign.empty() && assign.back()=='\n') assign.pop_back();
    if (!assign.empty() && assign.back()=='\r') assign.pop_back();

    cout << "ASSIGNMENT: " << assign << endl;

    // parse
    stringstream ss(assign);
    string op, a_str, b_str;
    if (!(ss >> op >> a_str >> b_str)) {
        close(sock);
        print_stdout("ERROR: RESOLVE ISSUE\n");
        return 1;
    }

    string result_str;
    bool is_float = false;

    try {
        if (op == "add" || op == "sub" || op == "mul" || op == "div") {
            long long a = stoll(a_str);
            long long b = stoll(b_str);
            long long r = 0;
            if (op == "add") r = a + b;
            else if (op == "sub") r = a - b;
            else if (op == "mul") r = a * b;
            else if (op == "div") {
                if (b == 0) r = 0;
                else r = a / b; // truncated integer division
            }
            result_str = to_string(r) + "\n";
        } else if (op == "fadd" || op == "fsub" || op == "fmul" || op == "fdiv") {
            is_float = true;
            double a = stod(a_str);
            double b = stod(b_str);
            double r = 0.0;
            if (op == "fadd") r = a + b;
            else if (op == "fsub") r = a - b;
            else if (op == "fmul") r = a * b;
            else if (op == "fdiv") {
                if (b == 0.0) r = 0.0;
                else r = a / b;
            }
            char buf[128];
            // format as %8.8g
            snprintf(buf, sizeof(buf), "%8.8g", r);
            result_str = string(buf) + "\n";
        } else {
            close(sock);
            print_stdout("ERROR: RESOLVE ISSUE\n");
            return 1;
        }
    } catch (...) {
        close(sock);
        print_stdout("ERROR: RESOLVE ISSUE\n");
        return 1;
    }

    // send result
    send(sock, result_str.c_str(), result_str.size(), 0);

    // read server response
    string verdict = readline(sock);
    if (verdict.empty()) {
        close(sock);
        print_stdout("ERROR: RESOLVE ISSUE\n");
        return 1;
    }
    // Trim newline + optional CR
    if (!verdict.empty() && verdict.back()=='\n') verdict.pop_back();
    if (!verdict.empty() && verdict.back()=='\r') verdict.pop_back();

    // print server response and our result in the format examples
    if (verdict == "OK") {
        cout << "OK (myresult=" << result_str.substr(0, result_str.size()-1) << ")" << endl;
    } else {
        cout << "ERROR (myresult=" << result_str.substr(0, result_str.size()-1) << ")" << endl;
    }

    close(sock);
    return 0;
}
