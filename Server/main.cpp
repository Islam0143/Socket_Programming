#include <iostream>
#include <winsock2.h>
#include <winsock.h>
#include <unistd.h>
#include <thread>
#include <bits/stdc++.h>

using namespace std;

int number_of_active_connections = 0;
CRITICAL_SECTION cs;
CRITICAL_SECTION cs2;
const int BASE_TIMEOUT = 20000; // 20 s

// Function to handle GET requests
void HandleGetRequest(SOCKET clientSocket, string fileName) {
    // Open the file in binary mode
    ifstream inputFile("../" + fileName, ios::binary);

    if (!inputFile.is_open()) {
        // If the file does not exist, send a 404 Not Found response
        cout << fileName <<" not exists in server" <<endl;
        string Not_Found_Message= "HTTP/1.1 404 Not Found\r\n" + fileName + "\r\n\r\n";
        const char* response = Not_Found_Message.c_str();
        send(clientSocket, response, Not_Found_Message.length(), 0);
    }
    else {
        // If the file exists, send a 200 OK response with file content
        string OK_Message= "HTTP/1.1 200 OK\r\n";
        string fileContent((istreambuf_iterator<char>(inputFile)), istreambuf_iterator<char>());
        string temp = OK_Message + "File-Name: " + fileName + "\r\n" + fileContent + "\r\n\r\n";
        const char* response = temp.c_str();
        send(clientSocket, response, temp.length(), 0);
        cout << fileContent.length() << endl;
    }
}

// Function to handle POST requests
void HandlePostRequest(SOCKET clientSocket, string filePath, istringstream& iss) {
    // Read data from the request until "endPOST" is encountered
    string data = "";
    string line;
    getline(iss, line, '\n');
    while(line != "endPOST") {
        data += line + '\n';
        getline(iss, line, '\n');
    }

    // Write the received data to a file
    ofstream outputFile("../" + filePath);
    if (!outputFile.is_open()) {
        cout << "Error opening file: " << filePath << endl;
    }
    // write data to file
    outputFile << data;
    outputFile.close();

    // Send a 200 OK response indicating successful upload
    string OK_Message= "HTTP/1.1 200 OK\r\n";
    string temp = OK_Message + filePath + " uploaded successfully\r\n\r\n";
    const char* response = temp.c_str();
    send(clientSocket, response, temp.length(), 0);
}

// Function to handle a TCP client
void HandleTCPClient(SOCKET clientSocket) {
    // hand shaking
    const char* message = "Hello from Server";
    send(clientSocket, message, strlen(message), 0);

    char buffer[4096];
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    buffer[bytesRead] = '\0';
    cout << buffer << endl;

    // Set timeout based on the number of active connections
    int timeout = BASE_TIMEOUT / number_of_active_connections;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(int));

    while((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytesRead] = '\0';

        // Update timeout based on the number of active connections
        timeout = BASE_TIMEOUT / number_of_active_connections;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(int));

        // stream to access buffer
        istringstream iss(buffer);
        string line;
        while (getline(iss, line, '\n')) {
            // stream to split line by spaces
            istringstream iss2(line);

            string arg;
            vector<string> args;

            while(iss2 >> arg)
                args.push_back(arg);

            // handle post request
            if(args[0] == "POST") {
                //EnterCriticalSection(&cs2);
                HandlePostRequest(clientSocket, args[1], iss);
                //LeaveCriticalSection(&cs2);
            }
            // handle get request
            else HandleGetRequest(clientSocket, args[1]);
        }
    }
    // client will disconnect so decrement number of active connection
    cout << "Client disconnected" << endl;
    EnterCriticalSection(&cs);
    number_of_active_connections--;
    LeaveCriticalSection(&cs);
    closesocket(clientSocket);
}

int main() {

    // Initialize Winsock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Failed to initialize Winsock" << endl;
        exit(-1);
    }

    // Create a socket for the server
    SOCKET serverSocket;
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cout << "Failed to create server socket" << endl;
        exit(-1);
    }

    // Set up the server address
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(80);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    memset(&serverAddress.sin_zero, 0, 8);

    // Bind the server socket to the server address
    if(bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(sockaddr)) != 0) {
        cout << "Failed to bind server socket to server address" <<endl;
        exit(-1);
    }

    // Listen for incoming connections
    if(listen(serverSocket, 20) != 0) {
        cout << "Server socket failed to Listen " << endl;
        exit(-1);
    }

    SOCKET clientSocket;
    InitializeCriticalSection(&cs);
    InitializeCriticalSection(&cs2);
    while(true) {
        cout << "Waiting for a client....." << endl;
        clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            cout << "Failed to accept client " << endl;
            continue;
        }
        cout << "A client connected successfully" << endl;
        // create thread to handle client requests
        thread(HandleTCPClient, clientSocket).detach();

        // increment number of connections
        EnterCriticalSection(&cs);
        number_of_active_connections++;
        LeaveCriticalSection(&cs);
    }
}
