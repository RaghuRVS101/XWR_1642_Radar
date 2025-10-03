// server.cpp
// Lab2a: line-based TCP server for TEXT TCP 1.0
// Build with: g++ -std=c++17 -O2 -Wall -o server server.cpp

#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

using namespace std;

static void perror_stdout(const string &s) {
    cout << s << endl;
}

static bool send_all(int fd, const string &s) {
    const char *p = s.c_str();
    size_t left = s.size();
    while (left > 0) {
        ssize_t n = send(fd, p, left, 0);
        if (n <= 0) return false;
        p += n;
        left -= n;
    }
    return true;
}

// Read a single line terminated by '\n'. Returns empty string on error/EOF.
static string recv_line(int fd, int timeout_seconds = -1) {
    string out;
    char c;
    // if timeout_seconds >= 0, we expect that socket already has SO_RCVTIMEO set or caller will handle
    while (true) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) return string(); // error or closed
        out.push_back(c);
        if (c == '\n') break;
    }
    return out;
}

// read until blank line (i.e. a '\n' line). Return entire accumulated string (may contain multiple lines)
static string recv_banner(int fd) {
    string accum;
    char buf[1024];
    while (true) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        accum.append(buf, buf + n);
        if (accum.find("\n\n") != string::npos || accum.find("\r\n\r\n") != string::npos) break;
        if (accum.size() > 16*1024) break; // safety
    }
    return accum;
}

static string trim_crlf(string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

static string rand_int_op() {
    static const vector<string> ops = {"add","sub","mul","div"};
    return ops[rand() % ops.size()];
}
static string rand_float_op() {
    static const vector<string> ops = {"fadd","fsub","fmul","fdiv"};
    return ops[rand() % ops.size()];
}

// generate random ints in a safe-ish range
static pair<int,int> random_ints() {
    int a = (rand() % 20001) - 10000; // -10000 .. 10000
    int b = (rand() % 20001) - 10000;
    if (b == 0) b = 1;
    return {a,b};
}
// generate random floats
static pair<double,double> random_floats() {
    double a = ((rand() % 1000000) / 1000000.0) * 200.0 - 100.0; // -100 .. 100
    double b = ((rand() % 1000000) / 1000000.0) * 200.0 - 100.0;
    if (fabs(b) < 1e-9) b = 1e-6;
    return {a,b};
}

int main(int argc, char **argv) {
    if (argc != 2) {
        cerr << "Usage: ./server <IP|DNS>:<PORT>\n";
        return 1;
    }

    // ignore SIGPIPE so send() failures don't kill server
    signal(SIGPIPE, SIG_IGN);

    string arg = argv[1];
    string host, port;
    if (!arg.empty() && arg.front() == '[') {
        // [ipv6]:port
        size_t pos = arg.find("]:");
        if (pos == string::npos) {
            perror_stdout("Bad address format");
            return 1;
        }
        host = arg.substr(1, pos-1);
        port = arg.substr(pos+2);
    } else {
        size_t pos = arg.rfind(':');
        if (pos == string::npos) {
            perror_stdout("Bad address format");
            return 1;
        }
        host = arg.substr(0, pos);
        port = arg.substr(pos+1);
    }

    // prepare addrinfo for bind (we accept both v4 & v6)
    struct addrinfo hints{}, *res = nullptr, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0) {
        perror_stdout(string("getaddrinfo: ") + gai_strerror(rc));
        return 1;
    }

    int listenfd = -1;
    for (rp = res; rp != nullptr; rp = rp->ai_next) {
        listenfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listenfd < 0) continue;
        int yes = 1;
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (bind(listenfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(listenfd);
        listenfd = -1;
    }
    freeaddrinfo(res);
    if (listenfd < 0) {
        perror_stdout("Failed to bind socket");
        return 1;
    }

    // backlog 5 as required
    if (listen(listenfd, 5) != 0) {
        perror_stdout("listen failed");
        close(listenfd);
        return 1;
    }

    cout << "server ready waiting for connections, on " << port << "  \n";

    srand((unsigned)time(nullptr) ^ getpid());

    // Main accept loop: one client at a time
    while (true) {
        struct sockaddr_storage cli_addr{};
        socklen_t cli_len = sizeof(cli_addr);
        int clientfd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_len);
        if (clientfd < 0) {
            if (errno == EINTR) continue;
            perror_stdout("accept failed");
            break;
        }

        // Handle the single client synchronously
        // 1) Send protocol lines and blank line
        if (!send_all(clientfd, string("TEXT TCP 1.0\n\n"))) {
            close(clientfd);
            continue;
        }

        // 2) Wait for client to respond with "OK\n" (we must read a line)
        // We'll set a 5s receive timeout for the client handshake/read/respond steps.
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        string clientline = recv_line(clientfd);
        if (clientline.empty()) {
            // client didn't respond
            close(clientfd);
            continue;
        }
        clientline = trim_crlf(clientline);
        if (clientline != "OK") {
            // client rejected protocol or sent something else -> close
            close(clientfd);
            continue;
        }

        // 3) generate assignment (random)
        bool isFloat = (rand() % 2 == 0); // half chance
        string assignment;
        if (isFloat) {
            string op = rand_float_op();
            auto pr = random_floats();
            double a = pr.first, b = pr.second;
            char buff[256];
            // format two floats with %8.8g and trailing newline
            snprintf(buff, sizeof(buff), "%s %8.8g %8.8g\n", op.c_str(), a, b);
            assignment = string(buff);
        } else {
            string op = rand_int_op();
            auto pr = random_ints();
            int a = pr.first, b = pr.second;
            char buff[256];
            snprintf(buff, sizeof(buff), "%s %d %d\n", op.c_str(), a, b);
            assignment = string(buff);
        }

        // send assignment
        if (!send_all(clientfd, assignment)) {
            close(clientfd);
            continue;
        }

        // 4) wait up to 5s for client answer (already set SO_RCVTIMEO)
        string answer = recv_line(clientfd);
        if (answer.empty()) {
            // timeout or closed; send ERROR TO\n and close
            send_all(clientfd, string("ERROR TO\n"));
            close(clientfd);
            continue;
        }
        answer = trim_crlf(answer);

        // compute server's correct result
        stringstream ss(assignment);
        string op; ss >> op;
        if (op.empty()) {
            send_all(clientfd, string("ERROR\n"));
            close(clientfd);
            continue;
        }

        bool server_ok = false;
        if (op[0] == 'f') {
            // floating
            double v1, v2;
            ss >> v1 >> v2;
            double expected = 0.0;
            if (op == "fadd") expected = v1 + v2;
            else if (op == "fsub") expected = v1 - v2;
            else if (op == "fmul") expected = v1 * v2;
            else if (op == "fdiv") expected = (fabs(v2) < 1e-12 ? 0.0 : v1 / v2);
            // parse client's double
            double clientv = 0.0;
            try {
                clientv = stod(answer);
            } catch (...) {
                clientv = NAN;
            }
            double d = fabs(expected - clientv);
            if (d < 0.0001) server_ok = true;
        } else {
            // integer ops
            long long v1, v2;
            ss >> v1 >> v2;
            long long expected = 0;
            if (op == "add") expected = v1 + v2;
            else if (op == "sub") expected = v1 - v2;
            else if (op == "mul") expected = v1 * v2;
            else if (op == "div") expected = (v2 == 0 ? 0 : v1 / v2);
            // parse client's integer
            long long clientv = 0;
            bool okparse = true;
            try {
                size_t idx = 0;
                clientv = stoll(answer, &idx);
                if (idx != answer.size()) okparse = false;
            } catch (...) {
                okparse = false;
            }
            if (okparse && clientv == expected) server_ok = true;
        }

        if (server_ok) send_all(clientfd, string("OK\n"));
        else send_all(clientfd, string("ERROR\n"));

        // done with this client
        close(clientfd);
    }

    close(listenfd);
    return 0;
}
