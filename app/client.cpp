#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>


static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

const uint32_t k_max_msg = 1048576;

std::string encodeBulkString(const std::string& str) {
    return "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
}

char* encodeRESP(const std::vector<std::string>& vec) {
    std::string encodedString;

    encodedString += "*" + std::to_string(vec.size()) + "\r\n";
    for (const auto& str : vec) {
        encodedString += encodeBulkString(str);
    }

    char* cstr = new char[encodedString.size() + 1];
    std::strcpy(cstr, encodedString.c_str());

    return cstr;
}

static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
    uint32_t len = 4;
    for (const std::string &s : cmd) {
        len += 4 + s.size();
    }
    if (len > k_max_msg) {
        return -1;
    }

    char* wbuf = encodeRESP(cmd);
    return write_all(fd, wbuf, 4 + len);
}

static int32_t read_res(int fd) {
    // 4 bytes header
    char rbuf[4 + k_max_msg + 1];
    rbuf[4 +k_max_msg] = '\0';

    // reply body
    ssize_t err = read(fd, rbuf, sizeof(rbuf));
    if (err == -1) {
        msg("read() error");
        return err;
    }

    // print the result
    uint32_t rescode = 0;
    memcpy(&rescode, &rbuf[4], 4);
    printf("server says: %s\n", rbuf);
    return 0;
}

int main(int argc, char **argv) {
    printf("argc: %d, argv: \n", argc);
    for (int i = 1; i < argc; ++i) {
        printf("%s\n", argv[i]);
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(6379);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr)
        <= 0) {
        printf(
            "\nInvalid address/ Address not supported \n");
        return -1;
    }
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect");
    }
    printf("here\n");

    std::vector<std::string> cmd;
    for (int i = 1; i < argc; ++i) {
        cmd.push_back(argv[i]);
        printf("adding cmd\n");
    }
    printf("sending request\n");
    int32_t err = send_req(fd, cmd);
    if (err) {
        goto L_DONE;
    }
    err = read_res(fd);
    if (err) {
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;
}