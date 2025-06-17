#pragma once
#include <iostream>
#include <vector>
#include <memory>

class Connection; // Forward declaration to avoid circular include

struct message_header
{
    char m_type = 0;                          // 1-name,2-chunk,3-message/command, 4-acknoledgement
    size_t m_size = 0;                        // name size or chunk size or message size - INITIALIZE!
    size_t chunk_id = 0;                      // only for chunk data - INITIALIZE!
    bool m_last_chunk = false;                // true for last - INITIALIZE!
    std::shared_ptr<Connection> m_connection; // for receiving

    // Constructor to ensure proper initialization
    message_header() = default;
};

struct message
{
    message_header m_header;
    std::vector<char> m_data; // name data, chunk data, message data

    // Constructor to ensure proper initialization
    message() = default;
};