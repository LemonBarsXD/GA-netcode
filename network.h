#pragma once
#include "packet.h"

int Net_InitClient(const char* ip, int port);

int Net_SendPacket(int fd, uint8_t type, void* data,
                uint16_t size);

int Net_ReceivePacket(int fd, struct header* out_header,
                void* out_data_buffer, int max_buffer_size);

void Net_Close(int fd);
