#include "message.h"
#include "tsqueue.h"
#include "connection.h"
#include "client.h"

void Send(Client &client, const std::string &line)
{
    message msg;
    msg.m_header.m_size = line.size();
    msg.m_data = std::vector<char>(line.begin(), line.end());
    client.Send(msg);
}

int main()
{
    try
    {
        std::string line;
        asio::io_context io;
        Client client(io);
        std::thread t([&io]()
                      { io.run(); });
        message m;

        client.Connect("127.0.0.1", "5555");

        while (true)
        {

            std::getline(std::cin, line);

            if (line == "Close")
            {
                client.Disconnect();
                break;
            }
            else if (line == "Send File")
            {
                std::string pathString;
                std::cout << "Enter File Path: ";
                std::getline(std::cin, pathString);
                std::cout << "Got File Path\n";
                client.SendFile(pathString);
            }
            else if (line.length() > 0)
            {
                client.SendString(line);
            }
        }
    }
    catch (std::error_code ec)
    {
        std::cout << ec.message() << "\n";
    }

    return 0;
}