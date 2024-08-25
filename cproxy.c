#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>

int checkIfNotExists(const char *path){
    return access(path, F_OK) != -1;
}

void displayFileContents(const char *fileName) {
    // Open file in read binary mode
    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        perror("file failed\n");
        exit(1);
    }

    // Counting all the bytes in the file
    fseek(file, 0, SEEK_END);
    unsigned long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Counting bytes for the response headers and the file content
    unsigned long totalBytes = fileSize + sizeof ("HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n");
    printf("HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n", fileSize);
    unsigned char buffer[256];
    size_t bytesRead;

    // Write the data to stdout
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        fwrite(buffer, 1, bytesRead, stdout);
    }

    printf("\n Total response bytes: %ld\n", totalBytes);
    fclose(file);
}

void openInBrowser(const char *openPath){
    char command[256];
    snprintf(command, sizeof(command), "xdg-open %s", openPath);
    int result = system(command);
    // Check if the command was executed successfully
    if (result == -1) {
        perror("Error executing command");
    }
}

char *saveToFileSystem(const char *host, const char *path){
    char *token;
    char *lastToken;
    // Duplicate the strings to not change the original once
    char hostCopy[256] = {0};
    strcpy(hostCopy, host);
    char pathCopy[256] = {0};
    strcpy(pathCopy, path);

    if (strlen(pathCopy) == 1){
        strcat(pathCopy, "index.html");
    }

    // Get the length required for the full path (+1 for null terminator)
    size_t fullPathLength = strlen(hostCopy) +strlen(pathCopy) + 1;
    char fullPath[fullPathLength];

    snprintf(fullPath, fullPathLength, "%s%s", hostCopy, pathCopy);

    char *saveptr1;
    char *saveptr2;

    token = strtok_r(fullPath, "/", &saveptr1);
    // This token is for knowing when we have reached the file instead of directories
    lastToken = strtok_r(pathCopy, "/", &saveptr2);
    while (token != NULL) {
        // Create directories until we reach the file
        if (lastToken != NULL) {
            // Check if the directory already exists
            if (!checkIfNotExists(token)) {
                mkdir(token, 0777); // Create the directory
            }
            chdir(token); // Change to the new directory
            lastToken = strtok_r(NULL, "/", &saveptr2);
        }
        else{
            // This is where the file should be saved, we check if we already have it
            if (checkIfNotExists(token)){
                displayFileContents(token);
                break;
            }
                // If not we send the name of the file to be created in the response
            else{
                return strdup(token);
            }
        }
        token = strtok_r(NULL, "/", &saveptr1);
    }

    return 0;
}

void startConnection(int sockfd, const char *host, int port){
    struct hostent* server_info = gethostbyname(host);
    struct sockaddr_in sock_info;

    // Use gethostbyname to translate host name to network byte order ip address
    if (!server_info){
        herror("gethostbyname failed\n");
        close(sockfd);
        exit(1);
    }

    // Set its attributes to 0 to avoid undefined behavior
    memset(&sock_info, 0, sizeof(sock_info));
    // Set the socket's port
    sock_info.sin_port = htons(port);
    // Set the type of the address to be IPv4
    sock_info.sin_family = AF_INET;
    // Set the socket's IP
    memcpy(&sock_info.sin_addr.s_addr, server_info->h_addr,server_info->h_length);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*) &sock_info, sizeof(sock_info)) < 0){
        perror("connect failed\n");
        close(sockfd);
        exit(1);
    }
}

void sendHttpRequest(int sockfd, const char *host, int port, const char *path) {
    // Construct the HTTP request
    char request[1024] = "\0";
    if (port == 80){
        snprintf(request, sizeof(request), "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host);
    }
    else{
        snprintf(request, sizeof(request), "GET %s HTTP/1.0\r\nHost: %s:%d\r\n\r\n", path, host, port);
    }
    printf("HTTP request =\n%s\nLEN = %ld\n", request, strlen(request));

    startConnection(sockfd, host, port);

    // Send the HTTP request
    if (write(sockfd, request, strlen(request)) < 0) {
        perror("request failed\n");
        close(sockfd);
        exit(1);
    }
}

void receiveHttpResponse(int sockfd, const char *host, const char *path, int *goodResponse){
    // Receive the HTTP response
    unsigned char *response = (unsigned char *)malloc(1024);
    ssize_t total_bytes = 0;
    int header_end = 0;
    int http_status_code = 0;
    FILE *file = NULL;
    char *fileName = NULL;

    printf("HTTP Response:\n");

    while (1) {
        ssize_t bytes_received = read(sockfd, response, 1024);
        if (bytes_received <= 0) {
            close(sockfd);
            break;
        }
        // Print the data byte by byte
        for (ssize_t i = 0; i < bytes_received; ++i) {
            printf("%c", response[i]);
            total_bytes++;
        }

        // Extract HTTP status code from the header
        if (http_status_code == 0){
            // Temporary char array for sscanf
            char tempBuffer[1024];
            memcpy(tempBuffer, response, bytes_received);
            tempBuffer[bytes_received] = '\0';

            sscanf(tempBuffer, "HTTP/1.%*d %d", &http_status_code);
            // Only create the file if the response was OK
            if (http_status_code == 200){
                *goodResponse = 1;
                // Create the path in the directory and get the file name that need to be saved
                fileName = saveToFileSystem(host, path);
                // Create file in write binary mode
                file = fopen(fileName, "wb");
                if (file == NULL) {
                    perror("file failed\n");
                    break;
                }
            }
        }

        // Check for the end of the header
        if (!header_end && http_status_code == 200) {
            for (ssize_t i = 0; i < bytes_received; ++i) {
                // Look for \r\n\r\n, signaling the end of the header
                if (i > 2 && response[i - 2] == '\r' && response[i - 1] == '\n' && response[i] == '\r' && response[i + 1] == '\n') {
                    header_end = 1;
                    // Write the rest of the response into the file
                    fwrite(response + i + 2, sizeof(unsigned char), bytes_received - i - 2, file);
                    break;
                }
            }
        }
        else {
            // Only if the response was OK we write to the file
            if (http_status_code == 200){
                fwrite(response, sizeof(unsigned char), bytes_received, file);
            }
        }
    }

    // Print the length of the response
    printf("\n Total response bytes: %ld\n", total_bytes);

    // Closing and clearing all the allocations we did
    if (http_status_code == 200){
        fclose(file);
        if (fileName){
            free(fileName);
        }
    }
    free(response);
}

void parseUrl(const char *url, char *hostName, int *port, char *filePath) {
    // Skip "http://"
    char *token = strtok((char *)url + 7, "/");
    char tempHost[256] = {0};

    if (token != NULL) {
        // Copy the host and the port, we will handle it later
        strcpy(tempHost, token);

        // Look for file path
        token = strtok(NULL, "");
        if (token != NULL) {
            sprintf(filePath, "/%s", token);
        }
    }

    // Parsing the host and port if exists
    token = strtok(tempHost, ":");
    if (token != NULL){
        strcpy(hostName, tempHost);
        // Check for port
        token = strtok(NULL, "");
        if (token != NULL){
            *port = atoi(token);
        }
        else{
            *port = 80;
        }
    }

    // If no path is specified, set default to "/"
    if (filePath[0] == '\0') {
        strcpy(filePath, "/");
    }
}

int main(int argc, char *argv[]) {
    const char *url = argv[1];
    char *urlCopy = NULL;
    char hostName[256] = {0};
    char filePath[256] = {0};
    char filePathCopy[256] = {0};
    int port = 0;
    int openBrowser = 0;
    int goodResponse = 0;

    // Check for correct format of the URL
    if (url == NULL || strncmp(url, "http://", 7) != 0){
        printf("Usage: cproxy <URL> [-s]\n");
        exit(1);
    }

    // Check for the -s flag
    if (argc == 3 && strcmp(argv[2], "-s") == 0) {
        openBrowser = 1;
        urlCopy = strdup(url);
    } else if (argc > 2) {
        perror("Usage: cproxy <URL> [-s]\n");
        exit(1);
    }

    // Parse the URL
    parseUrl(url, hostName, &port, filePath);

    strcpy(filePathCopy, filePath);

    // Get the full path, so we can check if the file already exists in our file system
    ssize_t pathLen = strlen(hostName) + strlen(filePath) + 1;

    // If no file is provided we search for index.html
    if (strlen(filePath) == 1){
        pathLen += strlen("index.html");
        strcat(filePathCopy, "index.html");
    }

    char fileCheck[pathLen];
    snprintf(fileCheck, sizeof(fileCheck), "%s%s", hostName, filePathCopy);

    // Create a socket with the address format of IPv4 over TCP
    int sockfd;
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed\n");
        exit(1);
    }

    // If we already have the file we get it directly from the file system
    if (checkIfNotExists(fileCheck)){
        printf("File is given from local filesystem\n");
        // Print the file on STDOUT
        saveToFileSystem(hostName, filePath);
        // Check the flag and open in browser if needed
        if (openBrowser) {
            openInBrowser(urlCopy);
        }
    }
        // If not we move to get it from the server
    else{
        sendHttpRequest(sockfd, hostName, port, filePath);
        receiveHttpResponse(sockfd, hostName, filePath, &goodResponse);
        if (openBrowser && goodResponse) {
            openInBrowser(urlCopy);
        }
    }

    free(urlCopy);
    close(sockfd);

    return 0;
}