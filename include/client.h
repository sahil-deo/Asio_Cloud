#pragma once
#include "asio.hpp"
#include "message.h"
#include "tsqueue.h"
#include "connection.h"
#include <iostream>
#include <thread>
#include <memory>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <chrono>

class Client
{
public:
    Client(asio::io_context &context) : m_context(context), m_socket(context), m_resolver(context)
    {
        readQueue = std::make_shared<TsQueue>();
    }
    ~Client()
    {
        Disconnect();
    }
    void Connect(std::string ip, std::string port)
    {
        try
        {
            auto endpoints = m_resolver.resolve(ip, port);
            /*
            asio::async_connect(m_socket, endpoints, [this, ip, port](std::error_code ec, asio::ip::tcp::endpoint e)
            {
                if (!ec)
                {
                m_connection = std::make_shared<Connection>(m_context, std::move(m_socket), readQueue);
                std::cout << "Connected to Server";
                Receive();

                }
                else
                {
                std::cout << "Connection Error";
            } });
                */

            std::error_code ec;

            asio::connect(m_socket, endpoints, ec);
            if (!ec)
            {
                m_connection = std::make_shared<Connection>(m_context, std::move(m_socket), readQueue);
                std::cout << "Connected to Server";
                Receive();
            }
            else
            {
                std::cout << "Connection Error";
            }
        }
        catch (std::exception e)
        {
            std::cout << e.what();
        }
    }

    void Receive()
    {
        if (!m_connection)
        {
            std::cout << "Cannot start receiving - no connection established\n";
            return;
        }
        std::thread([this, &readQueue = this->readQueue]()
                    {
                        while(true){
                            message msg = readQueue->pop_front();
                            
                            int messageType = msg.m_header.m_type;

                            if (messageType == 1){

                                if(m_file.is_open()){
                                    m_file.close();
                                }
                                //receive name
                                std::string fileName(msg.m_data.data(), msg.m_header.m_size);
                                m_file = std::ofstream(fileName, std::ios::binary);
                                SendAck(msg);


                            }else if (messageType == 2){
                                //receive file chunk
                                if(!m_file.is_open()){
                                    std::cout << "Error: File Chunk Received without receiving name\n";
                                    std::cout << "Receive Aborted\n";
                                    continue;
                                }

                                m_file.write(msg.m_data.data(), msg.m_header.m_size);
                                
                                if(msg.m_header.m_last_chunk){
                                    m_file.close();
                                    std::cout << "Received File";
                                }
                                SendAck(msg);


                            }else if (messageType == 3){
                                //receive message
                                std::vector<char> message(msg.m_data.begin(), msg.m_data.end());

                                for(char m: message){
                                    std::cout << m;
                                }
                                std::cout << "\n";
                            }else if(messageType == 4){
                                setAck();
                            }
                        } })
            .detach();
    }

    void Send(message msg)
    {
        if (!m_connection)
        {
            std::cout << "No connection established\n";
            return;
        }

        if (m_connection->Connected())
        {
            std::cout << "Inside Send Function\n";
            m_connection->Send(msg);
        }
        else
        {
            std::cout << "Disconnected from Server\n";
        }
    }

    void SendName(std::string fileName)
    {

        message msg;

        msg.m_header.m_type = 1;
        msg.m_header.m_size = fileName.size();

        msg.m_data.resize(msg.m_header.m_size);
        msg.m_data.assign(fileName.begin(), fileName.end());

        Send(msg);
    }

    void SendFile(std::string filePath)
    {
        if (!m_connection)
        {
            std::cout << "No connection established\n";
            return;
        }
        if (!m_connection->Connected())
        {
            std::cout << "Server not connected\n";
            return;
        }

        std::ifstream fileToSend(filePath, std::ios_base::binary);
        if (!fileToSend.is_open())
        {
            std::cout << "Failed to open file: " << filePath << "\n";
            return;
        }

        std::cout << "File Opened: " << filePath << "\n";

        std::string fileName = std::filesystem::path(filePath).filename().string();
        std::cout << "Sending filename: " << fileName << " (size: " << fileName.size() << ")\n";
        SendName(fileName);

        // Add a small delay to ensure name is sent first
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Prepare chunk message template
        message msg;
        msg.m_header.m_type = 2;           // chunk type
        msg.m_header.m_last_chunk = false; // Initialize properly

        const size_t CHUNK_SIZE = 100000;
        std::vector<char> buffer(CHUNK_SIZE);

        int chunkCount = 0;
        auto start = std::chrono::high_resolution_clock::now();
        falseAck();
        while (fileToSend.read(buffer.data(), CHUNK_SIZE) || fileToSend.gcount() > 0)
        {
            chunkCount++;
            size_t bytesRead = static_cast<size_t>(fileToSend.gcount());

            std::cout << "Sending chunk" << ", size: " << bytesRead << "\n";

            // Clear and prepare message for this chunk
            msg.m_data.clear();
            msg.m_data.assign(buffer.begin(), buffer.begin() + bytesRead);

            msg.m_header.m_size = bytesRead;

            // Check if this is the last chunk
            if (bytesRead < CHUNK_SIZE || fileToSend.eof())
            {
                msg.m_header.m_last_chunk = true;
                std::cout << "This is the last chunk\n";
            }
            else
            {
                msg.m_header.m_last_chunk = false;
            }

            Send(msg);

            if (checkAck(std::chrono::seconds(3)))
            {
                falseAck();
            }
            else
            {
                std::cout << "Enable to Receive Acknoledgement\n";
                std::cout << "Send Aborted";
                fileToSend.close();
                return;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
        fileToSend.close();
        std::cout << "File sending completed\n";
        std::cout << "Time Taken: " << duration.count() << "s\n";
    }

    void Sleep()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void SendString(std::string line)
    {
        message msg;
        msg.m_header.m_type = 3;
        msg.m_header.m_size = line.size();

        msg.m_data.assign(line.begin(), line.end());

        Send(msg);
    }

    void SendAck(message receivedMsg)
    {
        message msg;
        msg.m_header.m_type = 4;
        std::string ack = "true";
        msg.m_header.m_size = ack.size();
        msg.m_data.assign(ack.begin(), ack.end());
        Send(msg);
    }

    void Disconnect()
    {
        if (m_connection)
        {
            m_connection->Disconnect();
        }
    }

    bool Connected()
    {
        return m_connection && m_connection->Connected();
    }

    bool checkAck(std::chrono::seconds timeout)
    {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, timeout, [this]
                           { return m_ack; });
    }

    void setAck()
    {
        {

            std::lock_guard<std::mutex> lock(mtx);
            m_ack = true;
        }
        cv.notify_all();
    }

    void falseAck()
    {
        std::lock_guard<std::mutex> lock(mtx);
        m_ack = false;
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    std::shared_ptr<Connection> m_connection;
    asio::ip::tcp::resolver m_resolver;
    asio::ip::tcp::socket m_socket;
    std::shared_ptr<TsQueue> readQueue;
    asio::io_context &m_context;
    bool m_ack = false; // CHANGE: Initialize m_ack to false
    std::ofstream m_file;
};
