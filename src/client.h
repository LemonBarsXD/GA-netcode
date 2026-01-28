/*
 *  client util functions
 * */

#pragma once
#include "packet.h"

void cl_initqueue(client_t* client);

int cl_pushcmd(client_t* client, user_cmd_t* cmd);

int cl_popcmd(client_t* client, user_cmd_t* cmd);
