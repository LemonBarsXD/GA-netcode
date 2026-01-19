#pragma once
#include "packet.h"

int Net_Ping(int fd);

int Net_Recv(int fd, void* buffer, int bytes_needed);

int Net_InitClient(const char* ip, int port);

int Net_SendPacket(int fd, uint8_t type, void* data, uint16_t size);

int Net_Broadcast(client_t* clients, int amount, int fd_exclude, uint8_t type, void* data, uint16_t size);

int Net_ReceivePacket(client_t* client, header_t* out_header, void* out_data_buffer, int max_buffer_size);

void Net_Close(int fd);
