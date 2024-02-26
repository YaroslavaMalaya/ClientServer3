# Client-Server Chat App

## General System Description
This application is a multi-threaded chat system that allows users to communicate across different chat rooms using TCP connections.\
It supports features such as message sharing, file sharing, and room management (join, exit, and change rooms). The server can handle multiple clients concurrently.

Port: 12348\
IP Address: localhost (127.0.0.1)\
Input: Any message less than 1024 bytes, also commands (SEND anf EXIT). Each element (letter, sign, figure) in input is a char.\
Data Transfer Method: Binary\
Buffers and Chunks: no more than 1024 bytes\
Char: 1 byte (since the code is written in C++)\

## Application Protocol Description
Protocol Type: TCP (Transmission Control Protocol)\
Data Transfer Method: Binary\
Connection Establishment: Clients initiate TCP connections, which the server accepts, creating a new thread for each client.\
Choosing Room: Clients enter name and roomName, which the server handles, creating a new room or added in the existing.\
Command Exchange: Clients send message to another client or commands (SEND, EXIT) to the server, which processes accordingly.\
File Sharing: Client can share a file, and all other clients in the room can accept or decline receiving. For each client in the server side exist a special folders for storing sharing data with other users.\
Mutexes: Uses to ensure that when one thread is using a shared resource (<cout> in the application), other threads
are prevented from accessing the same resource at the same time. This avoids data corruption.\
Threads: Each client connection and room is handled in a separate thread, allowing for concurrent processing of client requests and different rooms.\
Rooms Usage: Each chat room is managed by the Room class, supporting real-time communication among clients. 
It includes a unique name, a thread for broadcasting messages, a list of client sockets, a message queue for pending messages, and synchronization mechanisms (std::mutex and std::condition_variable) to ensure thread safety.\

## Screenshots of Different Use Cases
### Join Room and Send Messages:
![Message Client1](pictures/message1.png)\
![Message Client2](pictures/message2.png)\
![MessageJoin Server Side](pictures/message.png)\

#### Create room and join client:
```cpp
Room* findCreateRoom(const std::string& roomName) {
        for (auto& room : rooms) {
            if (room->getNameRoom() == roomName) {
                return room.get();
            }
        }
        rooms.emplace_back(std::make_unique<Room>(roomName));
        return rooms.back().get();
    }
    
void Room::addClient(int clientSocket) {
    std::lock_guard<std::mutex> lock(mut);
    clients.push_back(clientSocket);
    std::cout << "CLIENT " << clientSocket << " JOINED ROOM " << name << std::endl;
    allClientsInTheRoom++;
}
```

#### Message queue ans display message:
```cpp
...
} else {
    Message message{content, clientName, " ", clientSocket};
    std::lock_guard<std::mutex> lock(roomsMutex);
    room->addMessageToQueue(message);
}
...

void Room::addMessageToQueue(const Message& message) {
    std::lock_guard<std::mutex> lock(mut);
    messageQueue.push(message);
    messageCondition.notify_one();
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
```

###  Send File, Accept File, Decline File:
![File Client1](pictures/file1.png)\
![File Client2](pictures/file2.png)\
![File Server Side](pictures/file3.png)\
![Decline Client1](pictures/decline1.png)\
![Decline Client2](pictures/decline2.png)\
![Decline Server Side](pictures/decline3.png)\
![Result](pictures/resultFiles.png)

#### Send file accept and decline it:
```cpp
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

 if (messageToStr.find("receive?") != std::string::npos) {
                size_t startPos = messageToStr.find("wants to send ") + std::string("wants to send ").length();;
                size_t endPos = messageToStr.find(" file");
                string filename = messageToStr.substr(startPos, endPos - startPos);
                messageToStr += "\nResponse (YES/NO and filename): ";

                cout << messageToStr;
                string response;
                getline(std::cin, response);
                send(clientSocket, response.c_str(), response.length(), 0);
            } else if (messageToStr.find("wait until all users receive") != std::string::npos) {
                cout << messageToStr << endl;
                std::unique_lock<std::mutex> lock(mut);
                canSend = false;
                allReceivedCondition.wait(lock, [this] { return !canSend; });
            } else if (messageToStr.find("\nAll users have received") == 0) {
                std::lock_guard<std::mutex> lock(mut);
                cout << messageToStr << endl;
                canSend = true;
                allReceivedCondition.notify_one();
                ....
                
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
```

###  Rejoining to Another Room:
![Rejoin Server Side](pictures/rejoin.png)\
![Rejoin Client1 Side](pictures/rejoin1.png)\
![Rejoin Client2 Side](pictures/rejoin2.png)

#### Rejoin:
```cpp
void handleRejoin(int clientSocket, std::string& clientName, std::string& roomName, Room* &room, bool& rejoin) {
        getNameAndRoom(clientSocket, clientName, roomName);
        std::lock_guard<std::mutex> lock(roomsMutex);
        room = findCreateRoom(roomName);
        room->addClient(clientSocket);
        rejoin = false;
    }
}

void Room::removeClient(int clientSocket) {
    std::lock_guard<std::mutex> lock(mut);
    clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
    std::cout << "CLIENT " << clientSocket << " LEFT THE ROOM " << name << std::endl;
    allClientsInTheRoom--;
}

void notifyRejoin(int clientSocket, std::string& roomName, Room* &room, bool& rejoin){
        std::lock_guard<std::mutex> lock(roomsMutex);
        room->removeClient(clientSocket);

        mutex.lock();
        std::cout << "Client " << clientSocket << " has left the room " << roomName << ". And will be rejoining to another." << std::endl;
        rejoin = true;
        mutex.unlock();
    }
```

## Explanation of Different Use Cases Using UML Diagram
###  Use Case Diagram:
![Use Case Diagram](pictures/diagram1.png)

###  Sequence  Diagram:
![Use Case Diagram](pictures/diagram2.png)
