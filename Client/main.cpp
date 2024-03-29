#include <iostream>
#include <winsock2.h>
#include <winsock.h>
#include <ws2tcpip.h>
#include <fstream>
#include <thread>
#include <bits/stdc++.h>
#include <unistd.h>

using namespace std;


// Function to check if a string starts with a specified prefix
bool startsWith(const string& str, const string& prefix) {
    if (str.length() < prefix.length()) return false;
    return str.compare(0, prefix.length(), prefix) == 0;
}

// Function to handle POST requests
void HandlePostRequest(string& command) {
    istringstream iss(command);
    string word;
    iss >> word;
    iss >> word;
    filesystem::path filePath(word);
    string fileName = filePath.filename().string();

    // Read the content of the file specified in the POST request
    ifstream inputFile("../" + fileName);
    if(!inputFile.is_open()) {
        cout << "cannot open file" <<endl;
        exit(-1);
    }
    string fileContent((istreambuf_iterator<char>(inputFile)), istreambuf_iterator<char>());
    command += "\r\n" + fileContent + "\nendPOST";
}

// Function to receive responses from the server
void receiveResponses(SOCKET clientSocket)
{
    char buffer[131072];
    int bytesRead;
    if ((bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytesRead] = '\0';
        cout << "bytesRead: " << bytesRead << endl;
        cout << "whole response: \n" << buffer << endl;

        // Find the position of "File-Name": in the buffer
        size_t fileNamePos = string(buffer).find("File-Name: ");

        // Check if the "File-Name": header is present in the response
        if (fileNamePos != string::npos)
        {
            // Extract the file name after the File-Name: header
            size_t startQuote = fileNamePos + strlen("File-Name: ");
            size_t endQuote = string(buffer).find("\r\n", startQuote);
            size_t nameSize = endQuote - startQuote;

            if (endQuote != string::npos)
            {
                string fileName = string(buffer).substr(startQuote, endQuote - startQuote);

                // TODO: Save the file with the extracted file name
                size_t newStartQuote = endQuote + strlen("\r\n");

                size_t contentSize = bytesRead - newStartQuote - 4;
                // Shift the remaining characters to fill the gap
                for (size_t i = 0; i < contentSize ; i++) {
                    buffer[i] = buffer[i + newStartQuote];
                }

                // Save content to file
                std::ofstream outputFile(fileName, std::ios::binary);

                if (outputFile.is_open()) {
                    // Write the data to the file
                    outputFile.write(buffer , contentSize);
                    outputFile.close();
                    cout << "Data saved to file: " << fileName << endl;
                } else {
                    cout << "Unable to open file: " << fileName << endl;
                }

                // display the file
                std::wstring name(fileName.begin(), fileName.end());

                // Use ShellExecute to open the image file with the default associated program
                ShellExecuteW(0, L"open", name.c_str() , 0, 0, SW_SHOW);
            }
            else
            {
                cout << "Invalid response format: Missing closing quote for File-Name" << endl;
            }
        }
        // TODO: Save the rest of the buffer to a file if needed
    }
}

int main() {
    // Initialize Winsock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Failed to initialize Winsock" << endl;
        exit(-1);
    }

    // Create a client socket
    SOCKET clientSocket;
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        cout << "Failed to create client socket" << endl;
        exit(-1);
    }

    // Set up the server address
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(80);
    inet_pton(AF_INET, "127.0.0.1", &(serverAddress.sin_addr));

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(struct sockaddr_in)) != 0) {
        cout << "Failed to connect to server" << endl;
        exit(-1);
    }

    cout << "Connected to server successfully" << endl;

    // Receive initial message from the server
    char buffer[65536];
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    buffer[bytesRead] = '\0';
    cout << buffer << endl;

    // Send a message to the server
    const char *message = "Hello from client";
    send(clientSocket, message, strlen(message), 0);

    // Open the commands file for reading
    ifstream inputFile("../commands.txt");
    if(!inputFile.is_open()) {
        cout << "cannot open file" << endl;
        exit(-1);
    }

    // Read commands from the file and send them to the server
    string command;
    while (getline(inputFile, command)) {
        // If the command is a POST request, handle it
        if (startsWith(command, "POST"))
            HandlePostRequest(command);

        // Append newline to the command
        command = command + "\n";
        const char* request = command.c_str();

        // Send the command to the server
        send(clientSocket, request, command.length(), 0);

        cout << command << endl;
        // receive the response from the server
        receiveResponses(clientSocket);
    }
    return 0;
}