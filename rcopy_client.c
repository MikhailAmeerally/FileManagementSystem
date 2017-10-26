#include <stdio.h>
#include <string.h>
#include "ftree.h"


#ifndef PORT
  #define PORT 30000
#endif

int main(int argc, char **argv) {
    // Note: In most cases, you'll want HOST to be 127.0.0.1, so you can
    // test on your local machine.
    char *dest;
    if (argc != 3 || (dest = strchr(argv[2], ':')) == NULL) {
        printf("Usage:\n\tfcopy SRC HOST_IP:DEST\n");
        return 1;
    }

    // Splitting argv[2] into IP and PATH
    *dest = '\0';
    dest = dest + 1;

    if (rcopy_client(argv[1], dest, argv[2], PORT) != 0) {
        printf("Errors encountered during copy\n");
        return 1;
    } else {
        printf("Copy completed successfully\n");
        return 0;
    }
}

