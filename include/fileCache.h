#pragma once

#include <filesystem>
#include <iostream>

struct file
{
    std::string name;
    std::filesystem::path path;
    size_t size;
    std::string extention;
};
