/*
 *  network util functions
 * */

#pragma once
#include "packet.h"

int net_ping(int fd);

int net_initserver(int port);

int net_initclient(const char* ip, int port);

int net_send(int fd, void* data, uint16_t size);

int net_sendpacket(int fd, uint8_t type, void* data, uint16_t size);

int net_broadcast(client_t* clients, int amount, int fd_exclude, uint8_t type, void* data, uint16_t size);

int net_recvpacket(client_t* client, header_t* out_header, void* out_data_buffer, int max_buffer_size);

void net_close(int fd);
