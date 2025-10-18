// Matchmaking Server
// This is a TCP-based matchmaking server that manages game sessions
// and connects players to dedicated game servers

#include <iostream>
#include <vector>
#include <list>
#include <thread>
#include <winsock2.h>    // Windows socket library
#include <ws2tcpip.h>    // Windows socket TCP/IP definitions

using namespace std;

// Maximum number of pending connections in the listen queue
#define MAXPENDING 5
// Buffer size for receiving data from clients
#define RCVBUFSIZE 1024

// Structure to store player connection information
typedef struct playerinfo {
    SOCKET client;  // Socket handle for the client connection
    int id;         // Unique player identifier
} PlayerInfo;

// Structure to store game session information
typedef struct sessioninfo {
    int id;          // Unique session identifier
    string name;     // Session name/title
    string serverip; // IP address of the game server hosting this session
    int serverport;  // Port number of the game server
} SessionInfo;

// Global list of all connected players
list<PlayerInfo> players;
// Global list of all active game sessions
list<SessionInfo> sessions;
// Counter for assigning unique player IDs
int playercount = 0;
// Counter for assigning unique session IDs
int sessioncount = 0;

// Parsing and Interpreting client messages:
// Message format: "CMD|param1|param2|...|#"
// Commands:
//   'g' - Get sessions: Client requests list of all active game sessions
//   'h' - Host session: Client wants to host a new game session
void InterpretClientMessage(char* buffer, PlayerInfo player)
{
    string temp = "";
    vector<string> params;
    char cmd = 0;

    // Parse the message buffer: extract command and parameters separated by '|'
    // Message is terminated by '#'
    for (int i = 0; buffer[i] != '#'; i++) {
        if ((buffer[i] == '|') && (cmd == 0)) {
            // First token is the command
            cmd = temp[0];
            temp = "";
        }
        else if ((buffer[i] == '|') && (cmd != 0)) {
            // Subsequent tokens are parameters
            params.push_back(temp);
            temp = "";
        }
        else {
            // Build up the current token character by character
            temp = temp + buffer[i];
        }
    }
    // Command 'g': Get list of all available game sessions
    if (cmd == 'g') {
        string message = "s|";
        if (sessions.size() > 0) {
            // Send each session's details to the client
            // Response format: "s|id|name|serverip|serverport|"
            for (list<SessionInfo>::iterator it = sessions.begin();
                it != sessions.end(); it++) {
                message = message + to_string(it->id) + "|" + it->name +
                    "|" + it->serverip + "|" +
                    to_string(it->serverport) + "|";
                if (send(player.client, message.c_str(), message.length(), 0)
                    == SOCKET_ERROR) {
                    cout << "send() failed" << endl;
                }
            }
        }
        else {
            // No sessions available, send null response
            message = message + "null|";
            if (send(player.client, message.c_str(), message.length(), 0)
                == SOCKET_ERROR) {
                cout << "send() failed" << endl;
            }
        }
    }
    // Command 'h': Host a new game session
    // Expected parameters: name, serverip, serverport
    else if (cmd == 'h') {
        SessionInfo session;
        session.id = sessioncount++;
        session.name = params.at(0);        // Session name from client
        session.serverip = params.at(1);    // Game server IP
        session.serverport = stoi(params.at(2)); // Game server port
        sessions.push_back(session);
        
        // Send confirmation back to client with assigned port
        // Response format: "o|serverport|"
        string message = "o|" + params.at(2) + "|";

        cout << "New session hosted: " << session.name
            << " at " << session.serverip << ":"
			<< session.serverport << endl;
        if (send(player.client, message.c_str(), message.length(), 0)
            == SOCKET_ERROR) {
            cout << "send() failed" << endl;
        }
    }
    else {
        // Unknown command received
        char* endline = strchr(buffer, '#');
        buffer[(int)(endline - buffer) + 1] = 0;
        cout << "Unknown message: " << buffer << endl;
    }
}

// Thread function to handle client connections:
// Each connected client gets its own thread running this function
// Continuously receives and processes messages from the client
void HandleClientThread(PlayerInfo player)
{
    char buffer[RCVBUFSIZE];
    // Keep receiving messages until the client disconnects
    while (recv(player.client, buffer, sizeof(buffer), 0) > 0) {
        InterpretClientMessage(buffer, player);
        memset(buffer, 0, sizeof(buffer)); // Clear buffer for next message
    }
    // Client disconnected, close the socket
    if (closesocket(player.client) == SOCKET_ERROR) {
        cout << "closesocket() failed" << endl;
    }
}

// Main function to initialize the server and wait for connections:
int main() {
    SOCKET server;
    SOCKADDR_IN server_addr, client_addr;
    WSADATA wsaData;

    // Step 1: Initialize Winsock library (version 2.0)
    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != NO_ERROR) {
        cout << "WSAStartup() failed" << endl;
        exit(EXIT_FAILURE);
    }
    
    // Step 2: Create TCP socket for the server
    if ((server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
        == INVALID_SOCKET) {
        cout << "socket() failed" << endl;
        exit(EXIT_FAILURE);
    }
    
    // Step 3: Configure server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;                   // IPv4
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // Accept connections on any network interface
    server_addr.sin_port = htons(8856);                 // Listen on port 8856

    // Step 4: Bind the socket to the address and port
    if (::bind(server, (struct sockaddr*)&server_addr,
        sizeof(server_addr)) == SOCKET_ERROR) {
        cout << "bind() failed" << endl;
        exit(EXIT_FAILURE);
    }
    
    // Step 5: Start listening for incoming connections
    if (listen(server, MAXPENDING) == SOCKET_ERROR) {
        cout << "listen() failed" << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Server Started!" << endl;

    // Main server loop: Accept and handle incoming client connections
    while (true) {
        SOCKET client;
        int clientlen = sizeof(client_addr);
        
        // Step 6: Accept a new client connection (blocking call)
        if ((client = accept(server, (struct sockaddr*)&client_addr,
            &clientlen)) == INVALID_SOCKET) {
            cout << "accept() failed" << endl;
        }

        // Convert client IP address to readable string format
        char addrstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), addrstr,
            INET_ADDRSTRLEN);
        cout << "Connection from " << addrstr << endl;

        // Create player info structure for the new client
        PlayerInfo cinfo;
        cinfo.client = client;
        cinfo.id = playercount++;  // Assign unique player ID
        players.push_back(cinfo);  // Add to global player list

        // Create a new thread to handle this client's messages
        // This allows the server to handle multiple clients concurrently
        thread* clientthread = new thread(HandleClientThread,
            cinfo);
    }
}
