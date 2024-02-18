#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include "file.h"
#include "room.h"
#include <filesystem>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
class MacServerConnection{
private:
    int serverSocket;
    struct sockaddr_in serverAddr;

public:
    MacServerConnection(int port) {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            error("Error creating socket");
        } else {
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = INADDR_ANY;
            serverAddr.sin_port = htons(port);

            if (bind(serverSocket, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) == -1) {
                error("Bind failed");
                close(serverSocket);
            }
        }
    }

    void error(const char* message) const {
        pthread_mutex_lock(&mutex);
        perror(message);
        pthread_mutex_unlock(&mutex);
    }

    int listenConnection() const {
        if (listen(serverSocket, SOMAXCONN) == -1) {
            error("Listen failed");
            return -1;
        }
        return 0;
    }

    int acceptConnection(struct sockaddr_in &clientAddr) const {
        socklen_t clientAddrLen = sizeof(clientAddr);
        return accept(serverSocket, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientAddrLen);
    }

    void sendM(int clientSocket, const char* buffer, size_t length, int num) const {
        send(clientSocket, buffer, length, num);
    }

    ssize_t recvM(int clientSocket, char* buffer, size_t length, int num) const {
        ssize_t receivedBytes = recv(clientSocket, buffer, length, num);
        return receivedBytes;
    }

    void closeConnection(){
        close(serverSocket);
    }
};

class Server{
private:
    int port = 12348;
    sockaddr_in clientAddr;
    MacServerConnection macServer;
    std::vector<std::thread> clientThreads;
    std::vector<std::unique_ptr<Room>> rooms;
    std::mutex roomsMutex;
    std::string dirFromCopy;

    void listenSocket(){
        if (macServer.listenConnection() == -1) {
            return;
        } else {
            std::cout << "Server listening on port " << port << std::endl;

            while (true) {

                int clientSocket = macServer.acceptConnection(clientAddr);
                if (clientSocket == -1) {
                    macServer.error("Error accepting client connection");
                    break;
                }

                pthread_mutex_lock(&mutex);
                std::cout << "Accepted connection from " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << std::endl;
                pthread_mutex_unlock(&mutex);

                clientThreads.emplace_back(&Server::handlingCommunication, this, clientSocket);
            }
        }
    }

public:
    Server() : macServer(port) {
        listenSocket();
    }

    ~Server() {
        macServer.closeConnection();
        for (auto& thread : clientThreads) {
            thread.join();
        }
    }

    void setPaths(std::string& clientName, std::string& clientFolderPath, std::string& serverFolderPath) {
        std::string baseFoldersPath = "/Users/Yarrochka/Mine/Study/CSC/clientserver3/ClientServer3/";
        clientFolderPath = baseFoldersPath + clientName;
        serverFolderPath = baseFoldersPath + "serverC" + clientName.substr(1);

        if (!std::__fs::filesystem::exists(clientFolderPath)) {
            if (std::__fs::filesystem::create_directories(clientFolderPath)) {
                std::cout << "Created client directory: " << clientFolderPath << std::endl;
            } else {
                std::cerr << "Failed to create client directory: " << clientFolderPath << std::endl;
            }
        }

        if (!std::__fs::filesystem::exists(serverFolderPath)) {
            if (std::__fs::filesystem::create_directories(serverFolderPath)) {
                std::cout << "Created server directory: " << serverFolderPath << std::endl;
            } else {
                std::cerr << "Failed to create server directory: " << serverFolderPath << std::endl;
            }
        }
    }

    void getNameAndRoom(int clientSocket, std::string& clientName, std::string& roomName){
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Failed to read client's name.\n";
            close(clientSocket);
            return;
        }
        clientName = std::string(buffer, bytesRead);

        memset(buffer, 0, sizeof(buffer));
        bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Failed to read room name.\n";
            close(clientSocket);
            return;
        }
        roomName = std::string(buffer, bytesRead);

        std::cout << "Client " << clientName << " will join to the room: " << roomName << std::endl;
    }

    Room* findCreateRoom(const std::string& roomName) {
        std::lock_guard<std::mutex> lock(roomsMutex);
        for (auto& room : rooms) {
            if (room->getNameRoom() == roomName) {
                return room.get();
            }
        }
        rooms.emplace_back(std::make_unique<Room>(roomName));
        return rooms.back().get();
    }

    void handlingCommunication(int clientSocket) {
        std::string roomName;
        std::string clientName;
        getNameAndRoom(clientSocket, clientName, roomName);

        std::string clientFolderPath;
        std::string serverFolderPath;
        setPaths(clientName, clientFolderPath, serverFolderPath);

        Room *room = findCreateRoom(roomName);
        room->addClient(clientSocket);
        std::string pathToFile;
        std::string pathToCopiedFile;

        while (true) {
            char buffer[1024];
            memset(buffer, 0, sizeof(buffer));
            ssize_t receivedBytes = macServer.recvM(clientSocket, buffer, sizeof(buffer), 0);
            if (receivedBytes > 0) {
                std::string content(buffer, receivedBytes);
                if (content == "EXIT") {
                    room->removeClient(clientSocket);
                    std::cout << "Client " << clientSocket << " has left the room " << roomName << std::endl;
                    getNameAndRoom(clientSocket, clientName, roomName);
                } else if (content.find("YES ") == 0) {
                    std::string filename = content.substr(4);
                    pathToFile = dirFromCopy;
                    pathToCopiedFile = clientFolderPath + "/" + filename;
                    File::copyFile(pathToFile, pathToCopiedFile, clientSocket);
                } else if (content.find("NO ") == 0) {
                    std::string filename = content.substr(3);
                    pathToFile = serverFolderPath + "/" + filename;
                    std::__fs::filesystem::remove(pathToFile);
                } else if (content.find("SEND ") == 0) {
                    std::string filename = content.substr(5);
                    pathToFile = clientFolderPath + "/" + filename;
                    pathToCopiedFile = serverFolderPath + "/" + filename;
                    File::copyFile(pathToFile, pathToCopiedFile, clientSocket);
                    dirFromCopy = pathToCopiedFile;
                    Message message{content, clientName, filename, clientSocket, room->nextId++};
                    room->addMessageToQueue(message);
                } else {
                    Message message{content, clientName, " ", clientSocket, room->nextId++};
                    room->addMessageToQueue(message);
                }
            } else {
                macServer.error("Received failed.");
                break;
            }
        }

        room->removeClient(clientSocket);
        close(clientSocket);
    }
};

int main() {
    Server newServer;
    return 0;
}