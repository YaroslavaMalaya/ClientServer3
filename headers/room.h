#ifndef CLIENTSERVER3_ROOM_H
#define CLIENTSERVER3_ROOM_H

#include <string>
#include <mutex>
#include <sys/socket.h>
#include <queue>
#include <thread>
#include <iostream>
#include <utility>

struct Message {
    std::string content;
    std::string clientName;
    int sendClientSocket;
    int id;
};

class Room {
public:
    std::string name;
    std::thread roomThread;
    std::vector<int> clients;
    std::queue<Message> messageQueue;
    std::mutex mut;
    std::condition_variable messageCondition;
    int nextId = 0;

    explicit Room(std::string name) : name(std::move(name)) {
        roomThread = std::thread(&Room::broadcastMessages, this);
    }

    void addClient(int clientSocket) {
        std::lock_guard<std::mutex> lock(mut);
        clients.push_back(clientSocket);
        std::cout << "CLIENT " << clientSocket << " JOINED ROOM " << name << std::endl;
    }

    void removeClient(int clientSocket) {
        std::lock_guard<std::mutex> lock(mut);
        clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
        std::cout << "CLIENT " << clientSocket << " GO OUT FROM THE ROOM " << name << std::endl;
    }

    void addMessageToQueue(const Message& message) {
        {
            std::lock_guard<std::mutex> lock(mut);
            messageQueue.push(message);
        }
        messageCondition.notify_one();
    }

    std::vector<int> getAllClients() const {
        return clients;
    }

    std::string getNameRoom() const {
        return name;
    }

    void broadcastMessages() {
        while (true)
        {
            std::unique_lock<std::mutex> lock(mut);
            messageCondition.wait(lock, [this]{ return !messageQueue.empty();});
            while (!messageQueue.empty())
            {
                Message message = messageQueue.front();
                messageQueue.pop();
                lock.unlock();
                for (int clientSocket : clients) {
                    if (clientSocket != message.sendClientSocket){
                        std::string messageContentName = "\n" + message.clientName + ": " + message.content;
                        send(clientSocket, messageContentName.c_str(), messageContentName.length(), 0);
                    }
                }
                lock.lock();
            }
            if (clients.empty()){
                break;
            }
        }
    }
};


#endif
