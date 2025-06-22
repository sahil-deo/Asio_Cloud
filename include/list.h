#include <mutex>
#include <vector>
#include <iostream>
#include <fstream>

class Connection;

template <typename T>
class List
{
public:
    List()
    {
        m_vector.clear();
    }

    ~List()
    {
        m_vector.clear();
    }

    void push_back(T element)
    {
        std::lock_guard<std::mutex> lock(mtx);
        m_vector.push_back(element);
    }

    T get_element(int index)
    {
        std::lock_guard<std::mutex> lock(mtx);
        return m_vector[index];
    }

    T pop_element(int index)
    {
        std::lock_guard<std::mutex> lock(mtx);
        T element = m_vector[index];
        m_vector.erase(m_vector.begin() + index);
        return element;
    }

    T pop_front(){
        std::lock_guard<std::mutex> lock(mtx);
        T element = m_vector[0];
        m_vector.erase(m_vector.begin());
        return element;
    }

    size_t size(){
        std::lock_guard<std::mutex> lock(mtx);
        return m_vector.size();
    }

    bool empty(){
        std::lock_guard<std::mutex> lock(mtx);
        return m_vector.empty();
    }

private:
    std::mutex mtx;
    std::vector<T> m_vector;
};

class message;

struct connectionFile
{
    message m_message;
    std::shared_ptr<Connection> m_connection;
    std::ifstream m_file;
};