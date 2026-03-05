#ifndef GAMINGCPU_VP_GDB_SERVER_H
#define GAMINGCPU_VP_GDB_SERVER_H

#include <systemc>
#include <cstdint>
#include <string>
#include <unordered_map>

class ISS;

// GDB RSP stub. Blocks on accept() until a client connects
class GDBServer : public sc_core::sc_module
{
public:
    GDBServer(sc_core::sc_module_name name, ISS& iss, uint16_t port);
    SC_HAS_PROCESS(GDBServer);

    bool is_enabled() const { return port_ != 0; }

private:
    void server_thread();
    void handle_client(int client_fd);

    std::string read_packet(int fd);
    void send_packet(int fd, const std::string& data);

    void handle_read_regs(int fd);
    void handle_write_regs(int fd, const std::string& data);
    void handle_read_mem(int fd, const std::string& data);
    void handle_write_mem(int fd, const std::string& data);
    void handle_insert_bp(int fd, const std::string& data);
    void handle_remove_bp(int fd, const std::string& data);

    static std::string to_hex32_le(uint32_t val);
    static uint32_t from_hex32_le(const std::string& hex, size_t offset);

    ISS& iss_;
    uint16_t port_;
    int server_fd_ = -1;

    // addr -> original instruction replaced by EBREAK
    std::unordered_map<uint32_t, uint32_t> breakpoints_;
    static constexpr uint32_t EBREAK_INSN = 0x00100073;
};

#endif // GAMINGCPU_VP_GDB_SERVER_H
