#include "client.h"


void
cl_initqueue(client_t* client)
{
    client->cmdqueue_len = 0;
    client->cmd_head = 0;
    pthread_mutex_init(&client->cmd_mutex, NULL);
}

int
cl_pushcmd(client_t* client, user_cmd_t* cmd)
{
    // returns:
    // 0  = success
    // -1 = queue is full

    pthread_mutex_lock(&client->cmd_mutex);
    if(client->cmdqueue_len >= TICK_RATE) {
        pthread_mutex_unlock(&client->cmd_mutex);
        return -1;
    }

    int tail = (client->cmd_head + client->cmdqueue_len) % TICK_RATE; // & TICK_RATE - 1

    client->cmdqueue[tail] = *cmd;
    client->cmdqueue_len++;

    pthread_mutex_unlock(&client->cmd_mutex);
    return 0;
}

int
cl_popcmd(client_t* client, user_cmd_t* cmd)
{
    // returns:
    // 1  = success
    // 0 = queue is empty

    pthread_mutex_lock(&client->cmd_mutex);

    if (client->cmdqueue_len <= 0) {
        pthread_mutex_unlock(&client->cmd_mutex);
        return 0;
    }

    *cmd = client->cmdqueue[client->cmd_head];

    client->cmd_head = (client->cmd_head + 1) % TICK_RATE; // & TICK_RATE - 1
    client->cmdqueue_len--;

    pthread_mutex_unlock(&client->cmd_mutex);
    return 1;
}
