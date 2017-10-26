#include <stdlib.h>
#include "hash.h"

#ifndef _FTREE_H_
#define _FTREE_H_

// Constants to set array sizes
#define MAXPATH 128
#define MAXDATA 256

// Constants that define client types
#define CHECKER_CLIENT 0
#define SENDER_CLIENT 1

// Constants that define responses
#define MATCH 0
#define MISMATCH 1
#define MATCH_ERROR 2

#define TRANSMIT_OK 0
#define TRANSMIT_ERROR 1

/* Struct for storing file information.
 */
struct fileinfo {
    char path[MAXPATH];
    mode_t mode;
    char hash[BLOCKSIZE];
    size_t size;
};

/* Client rcopy function.
 * Returns 0 on a successful copy and 1 otherwise.
 */
int rcopy_client(char *src_path, char *dest_path, char *host_ip, int port);

/* Server rcopy function.
 * Should never return.
 */
void rcopy_server(int port);

#endif // _FTREE_H_
