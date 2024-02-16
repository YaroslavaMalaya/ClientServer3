#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
using namespace std;

class Client{
private:
    int port = 12348;
    const char* serverIp = "127.0.0.1";
    int clientSocket;
    struct sockaddr_in serverAddr;

public:
    Client(){
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == -1) {
            perror("Error creating socket");
        }
        else{
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(port);
            inet_pton(AF_INET, serverIp, &(serverAddr.sin_addr));

            if (connect(clientSocket, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) == -1) {
                perror("Connect failed");
                close(clientSocket);
            }
        }
    }

    void receiveServerMessage() {
        char fileMessage[1024];
        memset(fileMessage, 0, sizeof(fileMessage));
        ssize_t bytes = recv(clientSocket, fileMessage, sizeof(fileMessage), 0);
        if (bytes > 0) {
            cout << fileMessage << endl;

        } else {
            cerr << "Failed to receive message from server." << endl;
        }
    }

    void sendClientName() const {
        string clientName;
        cout << "Enter your name: ";
        getline(cin, clientName);
        send(clientSocket, clientName.c_str(), clientName.size(), 0);
    }

    void chooseRoom() const {
        string roomName;
        cout << "Enter room name: ";
        getline(cin, roomName);
        send(clientSocket, roomName.c_str(), roomName.size(), 0);
    };

    void chat(){
        std::string message;
        while (true) {
            std::thread threadForMessagesReceiving(&Client::receiveServerMessage, this);
            threadForMessagesReceiving.detach();
            cout << "Enter the message: ";
            std::getline(std::cin, message);
            if (message == "EXIT") {
                break;
            }
            send(clientSocket, message.c_str(), message.size(), 0);
        }
    }

    ~Client() {
        close(clientSocket);
    }

};

int main() {
    Client newClient;
    newClient.sendClientName();
    newClient.chooseRoom();
    newClient.chat();
    return 0;
}
