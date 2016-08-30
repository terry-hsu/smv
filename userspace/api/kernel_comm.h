#ifndef KERNEL_COMM_H
#define KERNEL_COMM_H

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int message_to_kernel(char* message);
int get_family_id(int netlink_socket);
int send_to_kernel(int netlink_socket, const char *message, int length);
int compose_message(char* message);

#ifdef __cplusplus
}
#endif

#endif