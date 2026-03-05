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
        iss_.halt();
        wait(iss_.halted_event);
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

    // we don't validate checksums, life's too short
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

std::string GDBServer::to_hex32_le(uint32_t val) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(2) << ((val >> 0) & 0xFF)
       << std::setw(2) << ((val >> 8) & 0xFF)
       << std::setw(2) << ((val >> 16) & 0xFF)
       << std::setw(2) << ((val >> 24) & 0xFF);
    return ss.str();
}

uint32_t GDBServer::from_hex32_le(const std::string& hex, size_t offset) {
    auto byte = [&](size_t i) -> uint32_t {
        return std::stoul(hex.substr(offset + i * 2, 2), nullptr, 16);
    };
    return byte(0) | (byte(1) << 8) | (byte(2) << 16) | (byte(3) << 24);
}

void GDBServer::handle_read_regs(int fd) {
    std::string regs;
    for (int i = 0; i < 32; i++)
        regs += to_hex32_le(static_cast<uint32_t>(iss_.state.regs[i]));
    regs += to_hex32_le(iss_.state.pc);
    send_packet(fd, regs);
}

void GDBServer::handle_write_regs(int fd, const std::string& data) {
    for (int i = 0; i < 32 && (size_t)(i * 8 + 8) <= data.size(); i++)
        iss_.state.regs[i] = static_cast<int32_t>(from_hex32_le(data, i * 8));
    if (data.size() >= 33 * 8)
        iss_.state.pc = from_hex32_le(data, 32 * 8);
    iss_.state.regs[0] = 0;
    send_packet(fd, "OK");
}

void GDBServer::handle_read_mem(int fd, const std::string& data) {
    size_t comma = data.find(',');
    if (comma == std::string::npos) { send_packet(fd, "E01"); return; }

    uint32_t addr = std::stoul(data.substr(0, comma), nullptr, 16);
    uint32_t len = std::stoul(data.substr(comma + 1), nullptr, 16);

    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = iss_.bus_read(addr + i, 1) & 0xFF;
        result << std::setw(2) << static_cast<int>(byte);
    }
    send_packet(fd, result.str());
}

void GDBServer::handle_write_mem(int fd, const std::string& data) {
    size_t comma = data.find(',');
    size_t colon = data.find(':');
    if (comma == std::string::npos || colon == std::string::npos) {
        send_packet(fd, "E01");
        return;
    }

    uint32_t addr = std::stoul(data.substr(0, comma), nullptr, 16);
    std::string hex = data.substr(colon + 1);

    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        uint8_t byte = std::stoul(hex.substr(i, 2), nullptr, 16);
        iss_.bus_write(addr + i / 2, byte, 1);
    }
    send_packet(fd, "OK");
}

void GDBServer::handle_insert_bp(int fd, const std::string& data) {
    size_t comma1 = data.find(',');
    size_t comma2 = data.find(',', comma1 + 1);
    if (comma1 == std::string::npos) { send_packet(fd, "E01"); return; }

    uint32_t addr = std::stoul(data.substr(comma1 + 1,
        comma2 != std::string::npos ? comma2 - comma1 - 1 : std::string::npos),
        nullptr, 16);

    if (breakpoints_.count(addr) == 0) {
        uint32_t orig = iss_.bus_read(addr, 4);
        breakpoints_[addr] = orig;
        iss_.bus_write(addr, EBREAK_INSN, 4);
    }
    send_packet(fd, "OK");
}

void GDBServer::handle_remove_bp(int fd, const std::string& data) {
    size_t comma1 = data.find(',');
    size_t comma2 = data.find(',', comma1 + 1);
    if (comma1 == std::string::npos) { send_packet(fd, "E01"); return; }

    uint32_t addr = std::stoul(data.substr(comma1 + 1,
        comma2 != std::string::npos ? comma2 - comma1 - 1 : std::string::npos),
        nullptr, 16);

    auto it = breakpoints_.find(addr);
    if (it != breakpoints_.end()) {
        iss_.bus_write(addr, it->second, 4);
        breakpoints_.erase(it);
    }
    send_packet(fd, "OK");
}

void GDBServer::handle_client(int client_fd) {
    while (true) {
        std::string pkt = read_packet(client_fd);
        if (pkt.empty())
            break;

        if (pkt[0] == '?') {
            send_packet(client_fd, "S05");

        } else if (pkt[0] == 'g') {
            handle_read_regs(client_fd);

        } else if (pkt[0] == 'G') {
            handle_write_regs(client_fd, pkt.substr(1));

        } else if (pkt[0] == 'm') {
            handle_read_mem(client_fd, pkt.substr(1));

        } else if (pkt[0] == 'M') {
            handle_write_mem(client_fd, pkt.substr(1));

        } else if (pkt[0] == 's') {
            iss_.step();
            wait(iss_.halted_event);
            send_packet(client_fd, "S05");

        } else if (pkt[0] == 'c') {
            iss_.resume();
            wait(iss_.halted_event);
            send_packet(client_fd, "S05");

        } else if (pkt[0] == 'Z' && pkt.size() > 1 && pkt[1] == '0') {
            handle_insert_bp(client_fd, pkt.substr(1));

        } else if (pkt[0] == 'z' && pkt.size() > 1 && pkt[1] == '0') {
            handle_remove_bp(client_fd, pkt.substr(1));

        } else if (pkt[0] == 'k') {
            break;

        } else {
            send_packet(client_fd, "");
        }
    }
}
