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
    std::thread receiveThread;
    std::mutex mut;
    std::condition_variable allReceivedCondition;
    bool canSend = true;

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
            string messageToStr(fileMessage, bytes);
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
            } else {
                cout << messageToStr << endl;
            }
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
        string message;
        while (true) {
            if (receiveThread.joinable()) {
                receiveThread.join();
            }
            receiveThread = std::thread(&Client::receiveServerMessage, this);
            receiveThread.detach();

            cout << "Enter the message: ";
            getline(std::cin, message);
            if (message.find("REJOIN") == 0) {
                send(clientSocket, message.c_str(), message.size(), 0);
                cout << "\nYou can rejoin to another room." << endl;
                sendClientName();
                chooseRoom();
                continue;
            }
            if (message == "EXIT") {
                if (receiveThread.joinable()) {
                    receiveThread.join();
                }
                break;
            }
            send(clientSocket, message.c_str(), message.size(), 0);
        }
    }

    ~Client() {
        if (receiveThread.joinable()) {
            receiveThread.join();
        }
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
