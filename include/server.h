#pragma once
#include <vector>
#include <algorithm>
#include <thread>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

#include "ctime"
#include "asio.hpp"
#include "message.h"
#include "tsqueue.h"
#include "connection.h"
#include "fileCache.h"
#include "list.h"

class Server : public std::enable_shared_from_this<Server>
{
public:
    Server(asio::io_context &io, short Port)
        : m_context(io), m_acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), Port)), m_clientID(0) {}
    void StartServer()
    {
        std::cout << "Server Started\n";
        Accept();
        ReceiveMessage();
        ProcessSend();
        SendMessageThread();
    }
    void CloseServer() {}

    void Accept()
    {
        m_acceptor.async_accept(
            [this](std::error_code ec, asio::ip::tcp::socket newConnection)
            {
                m_clientID++;
                std::cout << "Client Accepted\n";

                if (newConnection.is_open())
                {
                    std::cout << "New Connection is Open\n";
                    m_clients.push_back(std::make_shared<Connection>(m_context, std::move(newConnection), readQueue, m_clientID));
                }
                else
                {
                    std::cout << "New connection not Open\n";
                }
                Accept();
            });
    }

    void SendMessage(message m)
    {
        m_clients.erase(
            std::remove_if(m_clients.begin(), m_clients.end(),
                           [&m](std::shared_ptr<Connection> &client)
                           {
                            if (!client->Connected())
                            {
                                //Implement Client Disconnect Properly if not connected.
                                client->Disconnect();
                                std::cout << "Client [" << client->Get_Endpoint() << "] Disconnected \n";
                                
                                return true;
                            }
                            else
                            {
                                client->Send(m);
                                return false;
                            } }),
            m_clients.end());
    }

    void SendMessageToClient(std::shared_ptr<Connection> connection, message msg)
    {
        if (!connection->Connected())
        {
            std::cout << "Unable to Send Message [Client is Disconnected]\n";
        }
        else
        {
            std::cout << "Calling Send on Connection\n";
            connection->Send(msg);
        }
    }

    void ReceiveMessage()
    {
        std::thread([this, &readQueue = this->readQueue]()
                    {
                        while(true){
                            message msg = readQueue->pop_front();
                            std::cout << "Received a message\n";

                            //check message type

                            int messageType = msg.m_header.m_type;
                            std::cout << "message type: " << messageType << "\n";

                            int64_t clientID = msg.m_header.m_connection->getID();
                            std::cout << "ClientID: " << clientID << "\n";
                            if (messageType == 1){ //Name Message

                                    std::string fileName(msg.m_data.data(), msg.m_header.m_size);
                                    clientFiles[clientID].open(fileName, std::ios_base::binary);

                            }else if(messageType == 2){ //Chunk Message
                                //std::string fileName(msg.m_data.data(), msg.m_header.m_size);
                                clientFiles[clientID].write(msg.m_data.data(), msg.m_header.m_size);

                                if(msg.m_header.m_last_chunk){
                                    
                                    clientFiles[clientID].close();
                                    clientFiles.erase(msg.m_header.m_connection->getID());
                                    std::cout << "Received File\n";
                                }
                                SendAck(msg);

                            }else if(messageType == 3){ //Message Message


                                m_processQueue.push_back(msg);
                                std::cout << "Sent to Process Queue\n";

                                // std::string message(msg.m_data.data(), msg.m_header.m_size); 
                                // if (message == "getdata"){
                                //     SendAvailableFiles(msg);
                                // }else if (message.length() > 4 && message.substr(0, 4) == "get "){
                                // // Fixed: Extract filename properly - remove "get " prefix
                                // std::string fileName = message.substr(4); 
                                // std::cout << "File requested: '" << fileName << "'\n";
                                // SendFile(msg);
                                // }else{
                                //     std::cout << message << "\n";
                                // }                     
                            }else if(messageType == 4){
                                setAck();
                            }
                        } })
            .detach();
    }

    void ProcessSend(){
        std::thread([this, &m_messageList = this->m_messageList, &m_nameList = this->m_nameList, &m_chunkList = this->m_chunkList](){

            while(true){
                message rcvdMsg = m_processQueue.pop_front();

                std::shared_ptr<connectionFile> msgcf = std::make_shared<connectionFile>();
                message msg;
                
                std::string messageString(rcvdMsg.m_data.data(), rcvdMsg.m_header.m_size);

                if(messageString == "getdata"){

                    messageString = GetAvailableFiles();
                    msg.m_data.assign(messageString.begin(), messageString.end());
                    msg.m_header.m_size = messageString.size();
                    msgcf->m_connection = rcvdMsg.m_header.m_connection;
                    msgcf->m_message = msg;

                    m_messageList.push_back(msgcf);
                    sahil
                
                }


                // std::string message(msg.m_data.data(), msg.m_header.m_size); 
                // if (message == "getdata"){
                //     SendAvailableFiles(msg);
                // }else if (message.length() > 4 && message.substr(0, 4) == "get "){
                // // Fixed: Extract filename properly - remove "get " prefix
                // std::string fileName = message.substr(4); 
                // std::cout << "File requested: '" << fileName << "'\n";
                // SendFile(msg);
                // }else{
                //     std::cout << message << "\n";
                // }                  

                
            }

        }).detach();
    }

    void SendMessageThread(){
        std::thread([this, &m_messageList = this->m_messageList, &m_nameList = this->m_nameList, &m_chunkList = this->m_chunkList](){
            while(true){
                if(!m_messageList.empty()){
                    clientFiles cf = m_messageList.pop_front();
                    SendMessageToClient(cf->m_connection, cf->m_message);
                }

                if(!m_nameList.empty()){

                }

                if(!m_chunkList.empty()){

                }
            }
        }).detach();
    }

    std::string GetAvailableFiles()
    {
        std::cout << "Available Files Requested:\n";
        // 1. Get Available Files and Folders
        updateFileCache();
        std::string message = "";
        for (file f : m_fileCache)
        {
            if (std::filesystem::is_regular_file(f.path)){
                std::cout << "IsRegularFile\n";
                std::cout << f.name << ":" << f.size;
                message.append(f.name + ":" + std::to_string(f.size) + "\n");
            }
        }

        return message;

        //msg.m_data = std::vector(message.begin(), message.end());

        //SendMessageToClient(receivedMsg.m_header.m_connection, msg);
    }

    void SendFile(message receivedMsg)
    {
        if (!receivedMsg.m_header.m_connection->Connected())
        {
            std::cout << "Client not connected\n";
            return;
        }

        std::string fileName(receivedMsg.m_data.data(), receivedMsg.m_header.m_size);

        fileName = fileName.substr(4, fileName.size() - 4);

        std::filesystem::path filePath;

        for (file f : m_fileCache)
        {
            if (f.name == fileName)
            {
                filePath = f.path;
                break;
            }
        }

        std::ifstream fileToSend(filePath, std::ios_base::binary);
        if (!fileToSend.is_open())
        {
            std::cout << "Failed to open file: " << filePath << "\n";
            return;
        }

        std::cout << "File Opened: " << filePath << "\n";

        // std::string fileName = std::filesystem::path(filePath).filename().string();
        std::cout << "Sending filename: " << fileName << " (size: " << fileName.size() << ")\n";
        SendName(fileName, receivedMsg.m_header.m_connection);

        // Add a small delay to ensure name is sent first
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Prepare chunk message template
        message msg;
        msg.m_header.m_type = 2;           // chunk type
        msg.m_header.m_last_chunk = false; // Initialize properly

        const size_t CHUNK_SIZE = 100000;
        std::vector<char> buffer(CHUNK_SIZE);

        int chunkCount = 0;
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

            SendMessageToClient(receivedMsg.m_header.m_connection, msg);

            // Small delay between chunks to prevent overwhelming
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));

            
            if (checkAck(std::chrono::seconds(20)))
            {
                falseAck();
            }
            else
            {
                std::cout << "Enable to receive Acknoledgement\n";
                std::cout << "Send Aborted\n";
                fileToSend.close();
                return;
            }
             
        }

        fileToSend.close();
        std::cout << "File sending completed\n";
    }

    void SendName(std::string fileName, std::shared_ptr<Connection> connection)
    {
        message msg;

        msg.m_header.m_type = 1;
        msg.m_header.m_size = fileName.size();

        msg.m_data.resize(msg.m_header.m_size);
        msg.m_data.assign(fileName.begin(), fileName.end());

        SendMessageToClient(connection, msg);
    }

    void SendAck(message receivedMsg)
    {
        message msg;
        msg.m_header.m_type = 4;
        std::string ack = "true";
        msg.m_header.m_size = ack.size();
        msg.m_data.assign(ack.begin(), ack.end());
        SendMessageToClient(receivedMsg.m_header.m_connection, msg);
    }

    void updateFileCache()
    {
        m_fileCache.clear();

        // Scan the current working directory for all files and folder

        for (auto path : std::filesystem::directory_iterator(std::filesystem::current_path()))
        {
            // for each path create a file object
            file tempFile;
            tempFile.path = path;
            tempFile.name = path.path().filename().string();
            tempFile.extention = path.path().extension().string();
            
            if(std::filesystem::is_regular_file(path)){
                tempFile.size = path.file_size();
            }else{
                tempFile.size = 0;
            }
            m_fileCache.push_back(tempFile);

            // store all file objects in the fileCache vector

        }
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

    std::vector<file> m_fileCache;
    std::vector<std::shared_ptr<Connection>> m_clients;
    asio::ip::tcp::acceptor m_acceptor;
    asio::io_context &m_context;
    std::shared_ptr<TsQueue> readQueue = std::make_shared<TsQueue>();
    std::unordered_map<int, std::ofstream> clientFiles;
    int m_clientID;
    std::mutex mtx;
    std::condition_variable cv;
    bool m_ack;

    List<std::shared_ptr<connectionFile>> m_messageList;
    List<std::shared_ptr<connectionFile>> m_nameList;
    List<std::shared_ptr<connectionFile>> m_chunkList;
    TsQueue m_processQueue;
};