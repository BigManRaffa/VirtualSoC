#ifndef GAMINGCPU_VP_GDB_SERVER_H
#define GAMINGCPU_VP_GDB_SERVER_H

#include <systemc>
#include <cstdint>

class ISS; // forward decl

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

    ISS& iss_;
    uint16_t port_;
    int server_fd_ = -1;
};

#endif // GAMINGCPU_VP_GDB_SERVER_H
