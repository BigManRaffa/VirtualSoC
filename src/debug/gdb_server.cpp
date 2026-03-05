#include "gdb_server.h"
#include "cpu/iss.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <iostream>

GDBServer::GDBServer(sc_core::sc_module_name name, ISS& iss, uint16_t port)
    : sc_module(name)
    , iss_(iss)
    , port_(port)
{
    if (port_ != 0)
        SC_THREAD(server_thread);
}

void GDBServer::server_thread() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[GDB] Failed to create socket\n";
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[GDB] Failed to bind port " << port_ << "\n";
        close(server_fd_);
        return;
    }

    listen(server_fd_, 1);
    std::cout << "[GDB] Listening on port " << port_ << "\n";

    int client = accept(server_fd_, nullptr, nullptr);
    if (client >= 0) {
        std::cout << "[GDB] Client connected\n";
        handle_client(client);
        close(client);
    }

    close(server_fd_);
}

std::string GDBServer::read_packet(int fd) {
    char c;
    while (read(fd, &c, 1) == 1 && c != '$') {}

    std::string payload;
    while (read(fd, &c, 1) == 1 && c != '#')
        payload += c;

    // Read 2-char checksum (we don't validate it, life's too short)
    char ck[2];
    read(fd, ck, 2);

    write(fd, "+", 1);
    return payload;
}

void GDBServer::send_packet(int fd, const std::string& data) {
    uint8_t cksum = 0;
    for (char c : data)
        cksum += static_cast<uint8_t>(c);

    std::ostringstream pkt;
    pkt << '$' << data << '#' << std::hex << std::setfill('0') << std::setw(2)
        << static_cast<int>(cksum);

    std::string s = pkt.str();
    write(fd, s.c_str(), s.size());

    char ack;
    read(fd, &ack, 1);
}

void GDBServer::handle_client(int client_fd) {
    while (true) {
        std::string pkt = read_packet(client_fd);
        if (pkt.empty())
            break;

        switch (pkt[0]) {
        case '?':
            send_packet(client_fd, "S05");
            break;

        case 'g': {
            std::ostringstream regs;
            for (int i = 0; i < 32; i++) {
                uint32_t val = static_cast<uint32_t>(iss_.state.regs[i]);
                regs << std::hex << std::setfill('0')
                     << std::setw(2) << ((val >> 0) & 0xFF)
                     << std::setw(2) << ((val >> 8) & 0xFF)
                     << std::setw(2) << ((val >> 16) & 0xFF)
                     << std::setw(2) << ((val >> 24) & 0xFF);
            }
            uint32_t pc = iss_.state.pc;
            regs << std::hex << std::setfill('0')
                 << std::setw(2) << ((pc >> 0) & 0xFF)
                 << std::setw(2) << ((pc >> 8) & 0xFF)
                 << std::setw(2) << ((pc >> 16) & 0xFF)
                 << std::setw(2) << ((pc >> 24) & 0xFF);
            send_packet(client_fd, regs.str());
            break;
        }

        case 'c':
            return;

        case 'k':
            return;

        default:
            send_packet(client_fd, "");

            break;
        }
    }
}
