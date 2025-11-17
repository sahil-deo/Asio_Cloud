# Asio Cloud

A high-performance client-server file-transfer system built in C++ using
Asio. Designed for home-network cloud storage where multiple clients can
upload files concurrently with detailed logging and predictable
performance.

## Motivation

This project was created to build a lightweight, backend-focused cloud
system without relying on third-party providers. It focuses on
performance, concurrency, and clean backend architecture.

## Features

-   Multi-client support using Asio TCP networking
-   Queuing for clients
-   File and folder uploads
-   Metadata: size, duration, timestamps
-   Detailed logging
-   CMake build system
-   Pure C++

## Architecture

### Server

-   Accepts multiple clients
-   Queues for client uploads and downloads
-   Saves incoming files
-   Logs events

### Client

-   Connect using server IP + password
-   Sends metadata + file payload
-   Displays transfer status

## Getting Started

### Prerequisites

-   C++17+
-   Asio / Boost.Asio
-   CMake
-   Modern compiler

### Build

    git clone https://github.com/sahil-deo/Asio_Cloud.git
    cd Asio_Cloud
    mkdir build && cd build
    cmake ..
    make

### Run

Server:

    ./server

Client:

    ./client

## Usage

### Server

-   Starts listening automatically
-   Saves incoming files
-   Logs all actions

### Client

-   Enter IP + password
-   Select file/folder
-   Send
