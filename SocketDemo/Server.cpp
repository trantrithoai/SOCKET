﻿#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define HAVE_STRUCT_TIMESPEC
#define PTW32_STATIC_LIB

#pragma comment(lib, "Ws2_32.lib")
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <pthread.h>
#include "proto.h"
#include "Server.h"
#include <WinSock2.h>

// Global variables
int server_sockfd = 0, client_sockfd = 0;
ClientList* root, * now;

void catch_ctrl_c_and_exit(int sig)
{
    ClientList* tmp;
    while (root != NULL) 
    {
        printf("\nClose socketfd: %d\n", root->data);
        closesocket(root->data); // close all socket include server_sockfd
        tmp = root;
        root = root->link;
        free(tmp);
    }
    printf("See you again!!!\n");
    exit(EXIT_SUCCESS);
}

void send_to_all_clients(ClientList* np, char tmp_buffer[]) 
{
    ClientList* tmp = root->link;
    while (tmp != NULL) 
    {
        if (np->data != tmp->data)  // all clients except itself.
        {
            send(tmp->data, tmp_buffer, LENGTH_SEND, 0);
        }
        tmp = tmp->link;
    }
}

void* client_handler(void* p_client) 
{
    int leave_flag = 0;
    char nickname[LENGTH_NAME] = {};
    char recv_buffer[LENGTH_MSG] = {};
    char send_buffer[LENGTH_SEND] = {};
    ClientList* np = (ClientList*)p_client;

    // Naming
    if (recv(np->data, nickname, LENGTH_NAME, 0) <= 0 || strlen(nickname) < 2 || strlen(nickname) >= LENGTH_NAME - 1) 
    {
        printf("%s didn't input name\n", np->ip);
        leave_flag = 1;
    }
    else 
    {
        strncpy(np->name, nickname, LENGTH_NAME);
        printf("%s join the chatroom\n", np->name);
        sprintf(send_buffer, "%s join the chatroom\n", np->name);
        send_to_all_clients(np, send_buffer);
    }

    // Conversation
    while (true)
    {
        if (leave_flag) 
        {
            break;
        }
        int receive = recv(np->data, recv_buffer, LENGTH_MSG, 0);
        if (receive > 0) 
        {
            if (strlen(recv_buffer) == 0) 
            {
                continue;
            }
            sprintf(send_buffer, "%s", recv_buffer);
            printf("%s\n", recv_buffer);
        }
        else if (receive == 0 || strcmp(recv_buffer, "exit") == 0) 
        {
            printf("%s leave the chatroom\n", np->name);
            sprintf(send_buffer, "%s leave the chatroom\n", np->name);
            leave_flag = 1;
        }
        else 
        {
            printf("%s leave the chatroom\n", np->name);
            sprintf(send_buffer, "%s leave the chatroom\n", np->name);
            leave_flag = 1;
        }
        send_to_all_clients(np, send_buffer);
    }

    // Remove Node
    closesocket(np->data);
    if (np == now) { // remove an edge node
        now = np->prev;
        now->link = NULL;
    }
    else { // remove a middle node
        np->prev->link = np->link;
        np->link->prev = np->prev;
    }
    free(np);
    return NULL;
}

int main()
{
    signal(SIGINT, catch_ctrl_c_and_exit);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("Failed. Error Code : %d", WSAGetLastError());
        return 1;
    }

    // Create socket
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd == -1) {
        printf("Fail to create a socket");
        exit(EXIT_FAILURE);

    }
    printf("Create Server successfull!!!\n");

    // Socket information
    struct sockaddr_in server_info, client_info;
    int s_addrlen = sizeof(server_info);
    int c_addrlen = sizeof(client_info);
    memset(&server_info, 0, s_addrlen);
    memset(&client_info, 0, c_addrlen);
    server_info.sin_family = PF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(8888); //Port=8888

    // Bind and Listen
    bind(server_sockfd, (struct sockaddr*)&server_info, s_addrlen);
    listen(server_sockfd, 5);

    // Print Server IP
    getsockname(server_sockfd, (struct sockaddr*)&server_info, &s_addrlen);
    printf("Start Server on port: %d\n", ntohs(server_info.sin_port));
    printf("=== WELCOME TO THE CHATROOM ===\n");

    // Initial linked list for clients
    root = newNode(server_sockfd, inet_ntoa(server_info.sin_addr));
    now = root;

    while (1) {
        client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_info, &c_addrlen);

        // Print Client IP
        getpeername(client_sockfd, (struct sockaddr*)&client_info, &c_addrlen);
        //printf("Client %s:%d come in.\n", inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));

        // Append linked list for clients
        ClientList* c = newNode(client_sockfd, inet_ntoa(client_info.sin_addr));
        c->prev = now;
        now->link = c;
        now = c;

        pthread_t id;
        if (pthread_create(&id, NULL, &client_handler, (void*)c) != 0) {
            perror("Create pthread error!\n");
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}

