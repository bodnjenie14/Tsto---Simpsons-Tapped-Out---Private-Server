#include <evpp/tcp_server.h>
#include <evpp/buffer.h>
#include <evpp/tcp_conn.h>
#include <evpp/udp/udp_server.h>
#include <evpp/udp/udp_message.h>
#include <evpp/http/http_server.h>

#ifdef _WIN32
#endif



void initialize_servers();
void initialize_backup_system();
void shutdown_backup_system();
