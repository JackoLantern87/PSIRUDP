#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"
#include "canvas.h"
#include "l_system.h"

/* ================= konfiguracja ================= */

#define CANVAS_W 60
#define CANVAS_H 60

/* ================= struktury ================= */

typedef struct {
    uint8_t node_id;
    struct sockaddr_in addr;
    int active;
} NodeEntry;

static NodeEntry nodes[MAX_NODES];
static int registered_nodes = 0;
static int last_node = -1;

/* L-system */
static char full_word[MAX_WORD];
static char word_parts[MAX_NODES][MAX_WORD_FRAGMENT];
static int parts_count = 0;
static int current_part = 0;

/* canvas globalny */
static Canvas *global_canvas;

/* aktualny stan żółwia */
static int turtle_x;
static int turtle_y;
static uint8_t turtle_dir;

/* ================= pomocnicze ================= */

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

static NodeEntry *next_node(void) {
    for (int i = 1; i <= MAX_NODES; i++) {
        int idx = (last_node + i) % MAX_NODES;
        if (nodes[idx].active) {
            last_node = idx;
            return &nodes[idx];
        }
    }
    return NULL;
}

/* prosty podział słowa */
static int split_word(const char *word) {
    int len = strlen(word);
    int base = len / registered_nodes;
    int offset = 0;

    for (int i = 0; i < registered_nodes; i++) {
        int n = (i == registered_nodes - 1)
                ? len - offset
                : base;
        memcpy(word_parts[i], word + offset, n);
        word_parts[i][n] = '\0';
        offset += n;
    }
    return registered_nodes;
}

/* ================= wysyłanie ================= */

static void send_task(
    int sockfd,
    NodeEntry *node,
    const char *word
) {
    uint8_t buffer[2048];

    TaskPayload task;
    task.start_x = htons(turtle_x);
    task.start_y = htons(turtle_y);
    task.direction = turtle_dir;
    task.word_len = htons(strlen(word));

    ProtocolHeader hdr = protocol_make_header(
        MSG_ASSIGN_TASK,
        PAYLOAD_TASK,
        node->node_id,
        sizeof(TaskPayload) + strlen(word)
    );
    hdr.payload_len = htons(hdr.payload_len);

    memcpy(buffer, &hdr, sizeof(hdr));
    memcpy(buffer + sizeof(hdr), &task, sizeof(task));
    memcpy(buffer + sizeof(hdr) + sizeof(task),
           word, strlen(word));

    sendto(sockfd, buffer,
           sizeof(hdr) + sizeof(task) + strlen(word),
           0,
           (struct sockaddr *)&node->addr,
           sizeof(node->addr));

    printf("➡ Wysłano fragment do node %d\n", node->node_id);
}

/* ================= main ================= */

int main(void) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    uint8_t buffer[4096];

    memset(nodes, 0, sizeof(nodes));

    /* ===== socket ===== */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    bind(sockfd,
         (struct sockaddr *)&server_addr,
         sizeof(server_addr));

    printf("🟢 Server UDP start\n");

    /* ===== L-system ===== */
    L_system lsys;
    lsystem_init(&lsys, "FX");
    lsystem_add_rule(&lsys, 'F', "FFFF[+FFFF][++FFFF][+++FFFF]");
    lsystem_add_rule(&lsys, 'X', "++X");
    lsystem_generate(&lsys, 2, full_word);

    printf("🌱 L-system wygenerowany (%lu znaków)\n",
           strlen(full_word));

    /* ===== canvas ===== */
    global_canvas = canvas_create(CANVAS_W, CANVAS_H);
    canvas_clear(global_canvas);

    turtle_x = CANVAS_W / 2;
    turtle_y = CANVAS_H / 2;
    turtle_dir = 0;

    /* ===== loop ===== */
    while (1) {
        ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&client_addr,
                               &addrlen);
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
                nodes[idx].active = 1;
                nodes[idx].addr = client_addr;
                nodes[idx].node_id = idx + 1;
                registered_nodes++;
                printf("✔ Node %d registered\n",
                       nodes[idx].node_id);
            }

            ProtocolHeader reply =
                protocol_make_header(MSG_REGISTER,
                                      PAYLOAD_EMPTY,
                                      nodes[idx].node_id,
                                      0);
            reply.payload_len = htons(0);

            sendto(sockfd, &reply, sizeof(reply), 0,
                   (struct sockaddr *)&client_addr,
                   addrlen);

            if (registered_nodes == MAX_NODES) {
                split_word(full_word);
                NodeEntry *n = next_node();
                send_task(sockfd, n, word_parts[0]);
            }
        }

        /* ===== TASK DONE ===== */
        if (hdr->msg_type == MSG_TASK_DONE &&
            hdr->p_type == PAYLOAD_CANVAS) {

            CanvasPayload *pl =
                (CanvasPayload *)(buffer + sizeof(ProtocolHeader));

            uint16_t canvas_len = ntohs(pl->canvas_len);
            uint8_t *canvas_data =
                buffer + sizeof(ProtocolHeader)
                + sizeof(CanvasPayload);

            Canvas *tmp =
                canvas_decode(canvas_data, canvas_len);

            merge_2_canvases(tmp, global_canvas);

            turtle_x = ntohs(pl->end_x);
            turtle_y = ntohs(pl->end_y);
            turtle_dir = pl->direction;

            canvas_destroy(tmp);

            current_part++;

            if (current_part < parts_count) {
                NodeEntry *n = next_node();
                send_task(sockfd, n,
                          word_parts[current_part]);
            } else {
                printf("\n🏁 FINALNY RYSUNEK:\n\n");
                canvas_print(global_canvas);
                break;
            }
        }
    }

    canvas_destroy(global_canvas);
    close(sockfd);
    return 0;
}
