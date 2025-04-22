#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <winsock2.h>
#include <unistd.h>

#define PORT 3880
#define MAX_BUFFER 1024

// Simulated frames (type = 0x01, payload = {0x10, 0x20}, checksum = 0x01 ^ 0x10 ^ 0x20 = 0x31)
uint8_t simulatedData[] = {
    0x7E, 0x02, 0x01, 0x10, 0x20, 0x31,    // Full frame
    0x7E, 0x02, 0x01,                      // Partial frame
    0x10, 0x20, 0x31                       // Continuation
};

int dataPos = 0;

// Simulate recv() by returning chunks of data
size_t mock_recv(uint8_t* dest, size_t maxLen) {
    if (dataPos >= sizeof(simulatedData)) return 0;

    size_t remaining = sizeof(simulatedData) - dataPos;
    size_t toRead = remaining > maxLen ? maxLen : remaining;

    memcpy(dest, &simulatedData[dataPos], toRead);
    dataPos += toRead;

    return toRead;
}

int parse_frame(const uint8_t* buffer, size_t size) {
    if (size < 5 || buffer[0] != 0x7E) {
        return 0;  // Too small or invalid start byte
    }

    uint8_t len = buffer[1];
    if (size != (size_t)(len + 4)) {
        return 0;  // Invalid size
    }

    uint8_t type = buffer[2];
    const uint8_t* payload = &buffer[3];
    uint8_t checksum = buffer[3 + len];

    // Calculate checksum
    uint8_t calculated = type;
    for (int i = 0; i < len; ++i) {
        calculated ^= payload[i];
    }

    if (calculated != checksum) {
        printf("Invalid checksum!\n");
        return 0;
    }

    // Output
    printf("Type: %d\n", type);
    printf("Payload: ");
    for (int i = 0; i < len; ++i) {
        printf("%02X ", payload[i]);
    }
    printf("\n");

    return 1;
}

void parse_stream(const uint8_t* stream, size_t tot_size){

    uint8_t *buffer;

    for (int i = 0 ; i < tot_size; i++ ){
        if (stream[i] != 0x7E){
            continue;
        }

        memcpy(buffer, &stream[i], stream[i + 1] + 4);
        int parsed = parse_frame(buffer, stream[i + 1] + 4);
        if (parsed == 0){
            printf("Incomplete or invalid frame at index %zu\n", i);
        }

        i += parsed;
    }
}


void handle_socket(int socket){
    uint8_t buffer[MAX_BUFFER];
    size_t buffLen = 0;

    while(1){
        SSIZE_T received = mock_recv(buffer + buffLen, MAX_BUFFER - buffLen);
        if (received <= 0){
            printf("Connection error.\n");
            break;
        }

        buffLen += received;

        for (int i = 0 ; i < buffLen; i++ ){
            if (buffer[i] != 0x7E){
                continue;
            }

            int parsed = parse_frame(&buffer[i], buffLen - i);
            if (parsed == 0){
                printf("Incomplete or invalid frame at index %zu\n", i);
            }

            i += parsed;
        }
    }
}

int main() {

    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in server;

    printf("\nInitialising Winsock...");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed. Error Code : %d",WSAGetLastError());
        return 1;
    }

    if (s = socket(AF_INET, SOCK_STREAM, 0) == INVALID_SOCKET)
    {
        printf("Could not create socket : %d.\n", WSAGetLastError());
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(s, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("Connection failed: %d.\n", WSAGetLastError());
        return 1;
    }

    printf("Connected to server.\n");
    handle_socket(s);

    close(s);
    return 0;
}