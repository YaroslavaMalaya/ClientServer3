#ifndef CLIENTSERVER3_ROOM_H
#define CLIENTSERVER3_ROOM_H

#include <string>
#include <mutex>
#include <vector>
#include <queue>
#include <thread>
#include <condition_variable>

struct Message {
    std::string content;
    std::string clientName;
    std::string filename;
    int sendClientSocket;
    bool isFile = false;
};

class Room {
private:
    friend class Server;
    std::string name;
    std::thread roomThread;
    std::vector<int> clients;
    std::queue<Message> messageQueue;
    std::mutex mut;
    std::condition_variable messageCondition;
    int allClientsInTheRoom = 0;

public:
    explicit Room(std::string name);
    ~Room();
    void addClient(int clientSocket);
    void removeClient(int clientSocket);
    void addMessageToQueue(const Message& message);
    std::string getNameRoom() const;
    void broadcastMessages();
};

#endif
