#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <memory>
#include <thread>
#include "file.h"
#include "room.h"
#include <filesystem>

std:: mutex mutex;
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
        std::lock_guard<std::mutex> lock(mutex);
        perror(message);
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

    static void sendM(int clientSocket, const char* buffer, size_t length, int num) {
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
    int port = 12349;
    sockaddr_in clientAddr;
    MacServerConnection macServer;
    std::vector<std::thread> clientThreads;
    std::vector<std::unique_ptr<Room>> rooms;
    std::mutex roomsMutex;
    std::string dirFromCopy;
    int sendClientSocket;

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

                mutex.lock();
                std::cout << "Accepted connection from " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << std::endl;
                mutex.unlock();

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
        mutex.lock();
        clientFolderPath = baseFoldersPath + clientName;
        serverFolderPath = baseFoldersPath + "serverC" + clientName.substr(1);
        mutex.unlock();

        if (!std::__fs::filesystem::exists(clientFolderPath)) {
            if (std::__fs::filesystem::create_directories(clientFolderPath)) {
                mutex.lock();
                std::cout << "Created client directory: " << clientFolderPath << std::endl;
                mutex.unlock();
            } else {
                std::string er = "Failed to create client directory: " + clientFolderPath;
                macServer.error(er.c_str());
            }
        }

        if (!std::__fs::filesystem::exists(serverFolderPath)) {
            if (std::__fs::filesystem::create_directories(serverFolderPath)) {
                mutex.lock();
                std::cout << "Created server directory: " << serverFolderPath << std::endl;
                mutex.unlock();
            } else {
                std::string er = "Failed to create server directory: " + serverFolderPath;
                macServer.error(er.c_str());
            }
        }
    }

    void getNameAndRoom(int clientSocket, std::string& clientName, std::string& roomName){
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            macServer.error("Failed to read client's name.\n");
            close(clientSocket);
            return;
        }
        mutex.lock();
        clientName = std::string(buffer, bytesRead);
        mutex.unlock();

        memset(buffer, 0, sizeof(buffer));
        bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            macServer.error("Failed to read room name.\n");
            close(clientSocket);
            return;
        }
        mutex.lock();
        roomName = std::string(buffer, bytesRead);
        std::cout << "Client " << clientName << " will join to the room: " << roomName << std::endl;
        mutex.unlock();
    }

    Room* findCreateRoom(const std::string& roomName) {
        for (auto& room : rooms) {
            if (room->getNameRoom() == roomName) {
                return room.get();
            }
        }
        rooms.emplace_back(std::make_unique<Room>(roomName));
        return rooms.back().get();
    }

    void handleRejoin(int clientSocket, std::string& clientName, std::string& roomName, Room* &room, bool& rejoin) {
        getNameAndRoom(clientSocket, clientName, roomName);
        std::lock_guard<std::mutex> lock(roomsMutex);
        room = findCreateRoom(roomName);
        room->addClient(clientSocket);
        rejoin = false;
    }

    void notifyRejoin(int clientSocket, std::string& roomName, Room* &room, bool& rejoin){
        std::lock_guard<std::mutex> lock(roomsMutex);
        room->removeClient(clientSocket);

        mutex.lock();
        std::cout << "Client " << clientSocket << " has left the room " << roomName << ". And will be rejoining to another." << std::endl;
        rejoin = true;
        mutex.unlock();
    }

    void acceptFile(int clientSocket, std::string& content, Room* &room, std::string& clientFolderPath){
        std::string filename = content.substr(4);

        mutex.lock();
        std::string pathToCopiedFile = clientFolderPath + "/" + filename;
        mutex.unlock();

        File::copyFile(dirFromCopy, pathToCopiedFile, clientSocket);

        std::lock_guard<std::mutex> lock(roomsMutex);
        room->allClientsInTheRoom--;

        if (room->allClientsInTheRoom == 1){
            std::string confirm = "\nAll users have received your file.";
            send(sendClientSocket, confirm.c_str(), confirm.length(), 0);
            room->allClientsInTheRoom = room->clients.size();
        }
    }

    void declineFile(std::string& content, Room* &room, std::string& serverFolderPath){
        std::string filename = content.substr(3);

        mutex.lock();
        std::string pathToFile = serverFolderPath + "/" + filename;
        std::__fs::filesystem::remove(pathToFile);
        mutex.unlock();

        std::lock_guard<std::mutex> lock(roomsMutex);
        room->allClientsInTheRoom--;

        if (room->allClientsInTheRoom == 1){
            std::string confirm = "\nAll users have received your file.";
            send(sendClientSocket, confirm.c_str(), confirm.length(), 0);
            room->allClientsInTheRoom = room->clients.size();
        }
    }

    void handleSendFile(int clientSocket, std::string& content, Room* &room, std::string& clientName, std::string& clientFolderPath, std::string& serverFolderPath){
        std::string filename = content.substr(5);
        mutex.lock();
        std::string pathToFile = clientFolderPath + "/" + filename;
        std::string pathToCopiedFile = serverFolderPath + "/" + filename;
        File::copyFile(pathToFile, pathToCopiedFile, clientSocket);

        dirFromCopy = pathToCopiedFile;
        sendClientSocket = clientSocket;
        mutex.unlock();

        Message message{content, clientName, filename, clientSocket, true};
        std::lock_guard<std::mutex> lock(roomsMutex);
        room->addMessageToQueue(message);

        std::string wait = "\nPlease wait until all users receive the file.";
        MacServerConnection::sendM(message.sendClientSocket, wait.c_str(), wait.length(), 0);
    }

    void handleExit(int clientSocket, std::string& roomName, Room* &room){
        std::lock_guard<std::mutex> lock(room->mut);
        room->removeClient(clientSocket);

        mutex.lock();
        std::cout << "Client " << clientSocket << " has left the room " << roomName << std::endl;
        mutex.unlock();
    }

    void handlingCommunication(int clientSocket) {
        std::string roomName;
        std::string clientName;
        getNameAndRoom(clientSocket, clientName, roomName);

        std::string clientFolderPath;
        std::string serverFolderPath;
        setPaths(clientName, clientFolderPath, serverFolderPath);

        Room *room = findCreateRoom(roomName);

        roomsMutex.lock();
        room->addClient(clientSocket);
        roomsMutex.unlock();

        bool rejoin = false;

        while (true) {
            if (rejoin) {
                handleRejoin(clientSocket, clientName, roomName, room, rejoin);
            } else {
                char buffer[1024];
                memset(buffer, 0, sizeof(buffer));
                ssize_t receivedBytes = macServer.recvM(clientSocket, buffer, sizeof(buffer), 0);
                if (receivedBytes > 0) {
                    std::string content(buffer, receivedBytes);
                    if (content == "REJOIN") {
                        notifyRejoin(clientSocket, roomName, room, rejoin);
                    } else if (content.find("YES ") == 0) {
                        acceptFile(clientSocket, content, room, clientFolderPath);
                    } else if (content.find("NO ") == 0) {
                        declineFile(content, room, serverFolderPath);
                    } else if (content.find("SEND ") == 0) {
                        handleSendFile(clientSocket, content, room, clientName, clientFolderPath, serverFolderPath);
                    } else if (content == "EXIT") {
                        handleExit(clientSocket, roomName, room);
                    } else {
                        Message message{content, clientName, " ", clientSocket};
                        std::lock_guard<std::mutex> lock(roomsMutex);
                        room->addMessageToQueue(message);
                    }
                } else {
                    macServer.error("Received failed.");
                    break;
                }
            }
        }

        roomsMutex.lock();
        room->removeClient(clientSocket);
        roomsMutex.unlock();
        close(clientSocket);
    }

};

int main() {
    Server newServer;
    return 0;
}
