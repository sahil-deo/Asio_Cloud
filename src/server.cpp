#include "message.h"
#include "tsqueue.h"
#include "connection.h"
#include "server.h"

int main()
{
    asio::io_context io;
    Server server(io, 5555);
    server.StartServer();
    io.run();
    return 0;
}