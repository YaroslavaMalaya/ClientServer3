#ifndef CLIENTSERVER3_ROOM_H
#define CLIENTSERVER3_ROOM_H

#include <string>
#include <mutex>
#include <sys/socket.h>
#include <queue>
#include <thread>
#include <iostream>
#include <utility>
#include <sys/stat.h>

struct Message {
    std::string content;
    std::string clientName;
    std::string filename;
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
    std::condition_variable allReceivedCondition;

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

//    std::vector<int> getAllClients() const {
//        return clients;
//    }

    void printAllClients() {
        std::cout << "Clients in the room: ";
        for (int clientSocket : clients) {
            std::cout << clientSocket << " ";
        }
        std::cout << std::endl;
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
                if (message.content.find("SEND ") == 0){
//                    struct stat info{};
                    std::cout << "Client " << message.sendClientSocket << " sends a file " << message.filename << std::endl;
                    lock.unlock();
                    for (int clientSocket : clients) {
                        if (clientSocket != message.sendClientSocket){
                            std::string askClient = "\nCLIENT " + message.clientName + " wants to send " + message.filename + " file, do you want to receive?";
                            send(clientSocket, askClient.c_str(), askClient.length(), 0);
                        }
                    }
                    lock.lock();
                } else {
//                    std::cout << "Client " << message.sendClientSocket << " sends a message " << message.content << std::endl;
                    std::cout << "WANT SEND " << std::endl;
                    printAllClients();
                    lock.unlock();
                    for (int clientSocket : clients) {
                        if (clientSocket != message.sendClientSocket){
                            std::cout << "SEND " << std::endl;
                            std::string messageContentName = "\n" + message.clientName + ": " + message.content;
                            send(clientSocket, messageContentName.c_str(), messageContentName.length(), 0);
                        }
                    }
                    lock.lock();
                }
            }
            if (clients.empty()){
                break;
            }
        }
    }
};


#endif
