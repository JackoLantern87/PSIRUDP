#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"
#include "l_system.h"

#define MAX_RESULT_LEN 4096
#define REQUIRED_NODES 3   /* <<< ILE WĘZŁÓW CZEKA SERWER */

/* =========================================================
 *  Struktury
 * ========================================================= */

typedef struct {
    uint8_t node_id;
    struct sockaddr_in addr;
    int active;
} NodeEntry;

static NodeEntry nodes[MAX_NODES];
static int registered_nodes = 0;

/* L-system */
static char full_word[MAX_RESULT_LEN];
static char word_parts[MAX_NODES][MAX_WORD_FRAGMENT];
static int parts_count = 0;
static int current_part = 0;

/* składanie wyniku */
static char final_word[MAX_RESULT_LEN];
static size_t final_len = 0;

/* round-robin */
static int last_node_index = -1;

/* =========================================================
 *  Pomocnicze
 * ========================================================= */

static int find_free_slot(void) {
    for (int i = 0; i < MAX_NODES; i++)
        if (!nodes[i].active)
            return i;
    return -1;
}

static int find_node(struct sockaddr_in *addr) {
    for (int i = 0; i < MAX_NODES; i++)
        if (nodes[i].active &&
            nodes[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            nodes[i].addr.sin_port == addr->sin_port)
            return i;
    return -1;
}

static NodeEntry *get_next_node(void) {
    for (int i = 1; i <= MAX_NODES; i++) {
        int idx = (last_node_index + i) % MAX_NODES;
        if (nodes[idx].active) {
            last_node_index = idx;
            return &nodes[idx];
        }
    }
    return NULL;
}

/* =========================================================
 *  Dzielenie słowa
 * ========================================================= */

static int split_word(
    const char *word,
    char parts[MAX_NODES][MAX_WORD_FRAGMENT],
    int node_count
) {
    int len = strlen(word);
    int base = len / node_count;
    int offset = 0;

    for (int i = 0; i < node_count; i++) {
        int copy_len = (i == node_count - 1)
                       ? len - offset
                       : base;

        strncpy(parts[i], word + offset, copy_len);
        parts[i][copy_len] = '\0';
        offset += copy_len;
    }

    return node_count;
}

/* =========================================================
 *  Wysyłanie zadania
 * ========================================================= */

static void send_assign_task(
    int sockfd,
    NodeEntry *node,
    const char *word
) {
    uint8_t buffer[1500];

    ProtocolHeader *hdr = (ProtocolHeader *)buffer;
    TaskPayload *task =
        (TaskPayload *)(buffer + sizeof(ProtocolHeader));
    char *word_buf =
        (char *)(buffer + sizeof(ProtocolHeader) + sizeof(TaskPayload));

    uint16_t word_len = strlen(word);

    *hdr = protocol_make_header(
        MSG_ASSIGN_TASK,
        PAYLOAD_TASK,
        node->node_id,
        sizeof(TaskPayload) + word_len
    );
    hdr->payload_len = htons(hdr->payload_len);

    task->start_x   = htons(0);
    task->start_y   = htons(0);
    task->direction = 0;
    task->word_len  = htons(word_len);

    memcpy(word_buf, word, word_len);

    size_t total_len =
        sizeof(ProtocolHeader) +
        sizeof(TaskPayload) +
        word_len;

    sendto(sockfd, buffer, total_len, 0,
           (struct sockaddr *)&node->addr,
           sizeof(node->addr));

    printf("→ Wysłano fragment %d do node %d: \"%s\"\n",
           current_part, node->node_id, word);
}

/* =========================================================
 *  MAIN
 * ========================================================= */

int main(void) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    uint8_t buffer[1500];

    memset(nodes, 0, sizeof(nodes));
    memset(final_word, 0, sizeof(final_word));

    /* ===== socket ===== */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("🟢 Serwer UDP działa na porcie %d\n", SERVER_PORT);
    printf("⏳ Oczekiwanie na %d node’y...\n", REQUIRED_NODES);

    /* ===== L-system ===== */
    L_system lsys;
    lsystem_init(&lsys, "FX");
    lsystem_add_rule(&lsys, 'F', "FFFF[+FFFF][++FFFF][+++FFFF]");
    lsystem_add_rule(&lsys, 'X', "++X");

    lsystem_generate(&lsys, 2, full_word);
    printf("🌱 L-system word: %s\n", full_word);

    /* ===== loop ===== */
    while (1) {
        ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (len < (ssize_t)sizeof(ProtocolHeader))
            continue;

        ProtocolHeader *hdr = (ProtocolHeader *)buffer;
        if (hdr->version != PROTOCOL_VERSION)
            continue;

        /* ===== REGISTER ===== */
        if (hdr->msg_type == MSG_REGISTER) {
            int idx = find_node(&client_addr);
            if (idx < 0) {
                idx = find_free_slot();
                if (idx < 0)
                    continue;

                nodes[idx].active = 1;
                nodes[idx].addr = client_addr;
                nodes[idx].node_id = idx + 1;
                registered_nodes++;

                printf("✔ Zarejestrowano node %d (%d/%d)\n",
                       nodes[idx].node_id,
                       registered_nodes,
                       REQUIRED_NODES);
            }

            ProtocolHeader reply =
                protocol_make_header(MSG_REGISTER,
                                      PAYLOAD_EMPTY,
                                      nodes[idx].node_id,
                                      0);
            reply.payload_len = htons(0);

            sendto(sockfd, &reply, sizeof(reply), 0,
                   (struct sockaddr *)&client_addr,
                   client_len);

            /* START DOPIERO GDY JEST KOMPLET */
            if (registered_nodes == REQUIRED_NODES && parts_count == 0) {
                parts_count = split_word(
                    full_word,
                    word_parts,
                    registered_nodes
                );
                current_part = 0;
                NodeEntry *node = get_next_node();
                send_assign_task(sockfd, node, word_parts[0]);
            }
        }

        /* ===== TASK DONE ===== */
        else if (hdr->msg_type == MSG_TASK_DONE) {
            uint16_t plen = ntohs(hdr->payload_len);
            char *data = (char *)(buffer + sizeof(ProtocolHeader));

            memcpy(final_word + final_len, data, plen);
            final_len += plen;
            final_word[final_len] = '\0';

            printf("✔ Node %d odesłał: \"%.*s\"\n",
                   hdr->node_id, plen, data);

            current_part++;

            if (current_part < parts_count) {
                NodeEntry *node = get_next_node();
                send_assign_task(sockfd, node,
                                 word_parts[current_part]);
            } else {
                printf("🏁 Wszystkie fragmenty odebrane\n");
                printf("✅ Finalne słowo: %s\n", final_word);
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
