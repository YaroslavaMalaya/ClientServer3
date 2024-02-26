#include "room.h"
#include <algorithm>
#include <iostream>
#include <sys/socket.h>

Room::Room(std::string name) : name(std::move(name)) {
    roomThread = std::thread(&Room::broadcastMessages, this);
}

Room::~Room() {
    if (roomThread.joinable()) {
        roomThread.join();
    }
}

void Room::addClient(int clientSocket) {
    std::lock_guard<std::mutex> lock(mut);
    clients.push_back(clientSocket);
    std::cout << "CLIENT " << clientSocket << " JOINED ROOM " << name << std::endl;
    allClientsInTheRoom++;
}

void Room::removeClient(int clientSocket) {
    std::lock_guard<std::mutex> lock(mut);
    clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
    std::cout << "CLIENT " << clientSocket << " LEFT THE ROOM " << name << std::endl;
    allClientsInTheRoom--;
}

void Room::addMessageToQueue(const Message& message) {
    std::lock_guard<std::mutex> lock(mut);
    messageQueue.push(message);
    messageCondition.notify_one();
}

std::string Room::getNameRoom() const {
    return name;
}

void Room::broadcastMessages() {
    while (true) {
        std::unique_lock<std::mutex> lock(mut);
        messageCondition.wait(lock, [this]{ return !messageQueue.empty(); });
        while (!messageQueue.empty()) {
            Message message = messageQueue.front();
            messageQueue.pop();
            if (message.isFile) {
                lock.unlock();
                clientsMutex.lock();
                for (int clientSocket : clients) {
                    if (clientSocket != message.sendClientSocket) {
                        std::string askClient = "\nCLIENT " + message.clientName + " wants to send " + message.filename + " file, do you want to receive?";
                        send(clientSocket, askClient.c_str(), askClient.length(), 0);
                    }
                }
                clientsMutex.unlock();
                lock.lock();
            } else {
                lock.unlock();
                clientsMutex.lock();
                for (int clientSocket : clients) {
                    if (clientSocket != message.sendClientSocket){
                        std::string messageContentName = "\n" + message.clientName + ": " + message.content;
                        send(clientSocket, messageContentName.c_str(), messageContentName.length(), 0);
                    }
                }
                clientsMutex.unlock();
                lock.lock();
            }
        }
        if (clients.empty()) {
            break;
        }
    }
}
