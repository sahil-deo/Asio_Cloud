// Fixed connection.h - Key changes to SendHeader and SendMessage
#pragma once
#include "message.h"
#include "tsqueue.h"
#include "asio.hpp"
#include <iostream>
#include <memory>

class Connection : public std::enable_shared_from_this<Connection>
{
public:
    Connection(asio::io_context &io, asio::ip::tcp::socket newSocket, std::shared_ptr<TsQueue> _readQ)
        : m_context(io),
          m_socket(std::move(newSocket)),
          m_readQueue(_readQ)
    {
        m_readQueue = std::move(_readQ);
        std::cout << "Connection Created\n";
        writing = false;
        if (m_socket.is_open())
        {
            Receive();
        }
    }

    Connection(asio::io_context &io, asio::ip::tcp::socket newSocket, std::shared_ptr<TsQueue> _readQ, int _id)
        : m_context(io),
          m_socket(std::move(newSocket)),
          m_readQueue(_readQ),
          m_connectionid(_id)
    {
        m_readQueue = std::move(_readQ);
        std::cout << "Connection Created\n";
        writing = false;
        if (m_socket.is_open())
        {
            Receive();
        }
    }

    void Send(message msg)
    {
        if (!m_socket.is_open())
        {
            std::cout << "Cannot send - socket is closed\n";
            return;
        }
        bool writingInProgress = !m_writeQueue.empty();
        m_writeQueue.push_back(std::move(msg));
        std::cout << "Send Called\n";
        asio::post(m_context, [self = shared_from_this(), writingInProgress]()
                   {
        if (!writingInProgress)
        {
            self->writing = true;
            self->SendHeader();
        } });
    }

    void SendHeader()
    {
        if (!m_socket.is_open())
        {
            std::cout << "Cannot send header - socket is closed\n";
            writing = false;
            return;
        }
        if (m_writeQueue.empty())
        {
            writing = false;
            return;
        }

        std::cout << "Sending Header\n";

        auto msg = std::make_shared<message>(m_writeQueue.pop_front());

        asio::async_write(m_socket, asio::buffer(&msg->m_header, sizeof(msg->m_header)),
                          [this, msg](std::error_code ec, size_t bytes_transferred)
                          {
                              if (!ec)
                              {
                                  std::cout << "Header sent successfully, size: " << bytes_transferred << "\n";
                                  SendMessage(msg);
                              }
                              else
                              {
                                  std::cout << "Error at Sending Header: " << ec.message() << "\n";
                                  writing = false;
                                  Disconnect();
                              }
                          });
    }

    void SendMessage(std::shared_ptr<message> msg)
    {
        if (!m_socket.is_open())
        {
            std::cout << "Cannot send message - socket is closed\n";
            writing = false;
            return;
        }
        std::cout << "Sending Message Body, size: " << msg->m_data.size() << "\n";

        if (msg->m_data.empty())
        {
            std::cout << "Warning: Sending empty message body\n";
            // Still continue to process next message
            if (!m_writeQueue.empty())
            {
                SendHeader();
            }
            else
            {
                writing = false;
            }
            return;
        }

        asio::async_write(m_socket, asio::buffer(msg->m_data.data(), msg->m_data.size()),
                          [this, msg](std::error_code ec, size_t bytes_transferred)
                          {
                              if (!ec)
                              {
                                  std::cout << "Message Sent successfully, size: " << bytes_transferred << "\n";
                                  if (!m_writeQueue.empty())
                                  {
                                      SendHeader();
                                  }
                                  else
                                  {
                                      writing = false;
                                  }
                              }
                              else
                              {
                                  std::cout << "Error Sending Message Body: " << ec.message() << "\n";
                                  writing = false;
                              }
                          });
    }

    void Receive()
    {
        if (!m_socket.is_open())
        {
            std::cout << "Cannot start receiving - socket is closed\n";
            return;
        }
        ReceiveHeader();
    }

    void ReceiveHeader()
    {
        if (!m_socket.is_open())
        {
            std::cout << "Cannot receive header - socket is closed\n";
            return;
        }
        auto msg = std::make_shared<message>();
        asio::async_read(m_socket, asio::buffer(&msg->m_header, sizeof(msg->m_header)),
                         [this, msg](std::error_code ec, size_t bytes_received)
                         {
                             if (!ec)
                             {
                                 std::cout << "Header received, type: " << static_cast<int>(msg->m_header.m_type)
                                           << ", size: " << msg->m_header.m_size << "\n";

                                 // Validate message size
                                 if (msg->m_header.m_size > 0 && msg->m_header.m_size < 1024 * 1024 * 10) // 10MB limit
                                 {
                                     ReceiveMessage(msg);
                                 }
                                 else if (msg->m_header.m_size == 0)
                                 {
                                     // Empty message - still valid, just add to queue
                                     std::cout << "Received empty message\n";
                                     msg->m_header.m_connection = shared_from_this();
                                     m_readQueue->push_back(*msg);
                                     ReceiveHeader();
                                 }
                                 else
                                 {
                                     std::cout << "Invalid message size: " << msg->m_header.m_size << "\n";
                                     ReceiveHeader(); // Skip this message and continue
                                 }
                             }
                             else
                             {
                                 std::cout << "Error receiving header: " << ec.message() << "\n";
                                 Disconnect();
                             }
                         });
    }

    void ReceiveMessage(std::shared_ptr<message> msg)
    {
        if (!m_socket.is_open())
        {
            std::cout << "Cannot receive message - socket is closed\n";
            return;
        }
        msg->m_data.resize(msg->m_header.m_size);
        asio::async_read(m_socket, asio::buffer(msg->m_data.data(), msg->m_data.size()),
                         [this, msg](std::error_code ec, size_t bytes_received)
                         {
                             if (!ec)
                             {
                                 std::cout << "Received Message Body, size: " << bytes_received << "\n";
                                 msg->m_header.m_connection = shared_from_this();
                                 m_readQueue->push_back(*msg);
                                 ReceiveHeader();
                             }
                             else
                             {
                                 std::cout << "Error receiving message body: " << ec.message() << "\n";
                                 Disconnect();
                             }
                         });
    }

    void Disconnect()
    {
        if (m_socket.is_open())
        {
            std::error_code ec;
            m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);

            m_socket.close(ec);
            if (ec)
            {
                std::cout << "Error closing socket: " << ec.message() << "\n";
            }
        }
    }

    bool Connected()
    {
        if (!m_socket.is_open())
        {
            return false;
        }
        std::error_code ec;
        m_socket.remote_endpoint(ec);
        return !ec;
    }

    asio::ip::tcp::endpoint Get_Endpoint()
    {
        return m_socket.remote_endpoint();
    }

    int getID()
    {
        return m_connectionid;
    }

private:
    asio::ip::tcp::socket m_socket;
    asio::io_context &m_context;
    std::shared_ptr<TsQueue> m_readQueue;
    TsQueue m_writeQueue;
    bool writing;
    int m_connectionid = 0;
};