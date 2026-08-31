#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "net.h"
#include "ssh.h"

int server(int port);
int client(char auth[], char address[], int port);

/// @brief splits a string based on a specified pattern, 
/// returns an array of strings on success, NULL on failure
/// @param string 
/// @param pattern 
/// @return array of strings 
char **str_spit(char *string, char* pattern);

/// @brief checks if the string contains a specified pattern
/// returns 0 if false, 1 if the condition has been met.
/// @param string 
/// @param pattern 
/// @return 0 = false, 1 = true
int str_contains(char *string, char* pattern);

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: coalesce serve <port>\n"
                        "       coalesce connect username:port@address:port\n");
        return 1;
    }


    if (net_init() != 0) {
        fprintf(stderr, "WSAStartup failed: %s\n", net_get_error());
        return 1;
    } 

    if (strcmp(argv[1], "serve") == 0) {
        int port = 22;
        if (argc > 2)
            port = atoi(argv[2]);
        server(port);

    } else if (strcmp(argv[1], "connect") == 0) {
        if (argc < 3 || !str_contains(argv[2], "@")) {
            fprintf(stderr, "usage: coalesce connect username:port@address:port\n");
            return 1;
        }

        char **set = str_spit(argv[2], "@");
        char *auth = set[0];

        char **hp = str_spit(set[1], ":");
        int result = client(auth, hp[0], atoi(hp[1]));

        free(hp);
        free(set);

        if (result != 0) {
            fprintf(stderr, "failed to connect to %s:%s\n", hp[0], hp[1]);
            return 1;
        }

    } else {
        fprintf(stderr, "unknown command: %s\n", argv[1]);
        fprintf(stderr, "usage: coalesce serve <port>\n"
                        "       coalesce connect username:port@address:port\n");
        return 1;
    }
    net_shutdown();
    return 0;
}   


int server(int port){

    net_socket_t fd = net_listen("0.0.0.0", port);
    if(NET_INVALID == fd){
        fprintf(stderr, "net_listen: failed to listern at port %d", port);
        fprintf(stderr, "\r\n%s",net_get_error());
        return -1;
    }
    struct net_addr *out_addr = {0};
    net_socket_t client = net_accept(fd, out_addr);

    if(NET_INVALID == client){
        fprintf(stderr, "net_accept: failed to accept new client");
        fprintf(stderr, "\r\n%s",net_get_error());
        return -1;
    }

    if (net_write_full(client, IDENT_STRING, sizeof(IDENT_STRING) - 1) < 0) {
        fprintf(stderr, "write: %s\n", net_get_error());
        net_close(client);
        return -1;
    }

    char in[1024];
    if(0 > net_read_line(client,in,1024))
    {
        fprintf(stderr, "read: %s\n", net_get_error());
        net_close(client);
        return -1;
    }

    printf("\r\nclient: %s",in);

    //send version ident
    return 0;
}

int client(char auth[], char address[], int port)
{
    net_socket_t s = net_connect(address, port);
    if (NET_INVALID == s) {
        fprintf(stderr, "net_connect: %s\n", net_get_error());
        return -1;
    }

    /* send identification */
    if (net_write_full(s, IDENT_STRING, sizeof(IDENT_STRING) - 1) < 0) {
        fprintf(stderr, "write: %s\n", net_get_error());
        net_close(s);
        return -1;
    }

    /* read server identification */
    char line[256];
    if (net_read_line(s, line, sizeof line) <= 0) {
        fprintf(stderr, "read: %s\n", net_get_error());
        net_close(s);
        return -1;
    }

    printf("server: %s", line);

    net_close(s);
    return 0;
}   



/* ── str_contains ────────────────────────────────────────────── */
int str_contains(char *string, char *pattern)
{
    if (!string || !pattern || !*pattern) return 0;
    return strstr(string, pattern) != NULL;
}

/* ── str_spit ────────────────────────────────────────────────── */
char **str_spit(char *string, char *pattern)
{
    if (!string || !pattern || !*pattern) return NULL;

    /* count tokens */
    size_t count = 1;
    for (char *p = string; *p; p++)
        if (*p == pattern[0]) count++;

    /* allocate result array + one extra for NULL sentinel */
    char **out = calloc(count + 1, sizeof *out);
    if (!out) return NULL;

    /* tokenize in-place (modifies `string`) */
    char *save = NULL;
    char *tok  = strtok_r(string, pattern, &save);
    size_t i = 0;

    while (tok) {
        out[i++] = tok;
        tok = strtok_r(NULL, pattern, &save);
    }
    /* out[i] is already NULL from calloc — sentinel */

    return out;
}   