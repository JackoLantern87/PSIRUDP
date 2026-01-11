#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"
#include "canvas.h"

/* ================= konfiguracja ================= */

#define SERVER_IP   "192.168.56.101"
#define CANVAS_W    60
#define CANVAS_H    60

/* ================= struktury ��wia ================= */

typedef enum {
    UP, UR, RIGHT, DR, DOWN, DL, LEFT, UL
} Direction;

typedef struct {
    int x;
    int y;
    Direction dir;
} TurtleState;

/* ================= rysowanie ================= */

static void draw_turtle(
    Canvas *canvas,
    const char *word,
    TurtleState *state
) {
    int dx[8] = { 0, 1, 1, 1, 0,-1,-1,-1 };
    int dy[8] = {-1,-1, 0, 1, 1, 1, 0,-1 };

    for (int i = 0; word[i]; i++) {
        switch (word[i]) {
            case 'F':
                state->x += dx[state->dir];
                state->y += dy[state->dir];
                canvas_set_cell(canvas, state->x, state->y, '#');
                break;

            case '+':
                state->dir = (state->dir + 1) % 8;
                break;

            case '-':
                state->dir = (state->dir + 7) % 8;
                break;

            default:
                break;
        }
    }
}

/* ================= main ================= */

int main(void) {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addrlen = sizeof(server_addr);
    uint8_t buffer[2048];
    uint8_t node_id = 0;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    /* ===== register ===== */
    ProtocolHeader reg = protocol_make_header(
        MSG_REGISTER, PAYLOAD_EMPTY, 0, 0
    );
    reg.payload_len = htons(0);

    sendto(sockfd, &reg, sizeof(reg), 0,
           (struct sockaddr *)&server_addr,
           sizeof(server_addr));

    printf("🔹 Node: wysłano MSG_REGISTER\n");

    while (1) {
        ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&server_addr,
                               &addrlen);
        if (len < (ssize_t)sizeof(ProtocolHeader))
            continue;

        ProtocolHeader *hdr = (ProtocolHeader *)buffer;
        if (hdr->version != PROTOCOL_VERSION)
            continue;

        /* ===== REGISTER ACK ===== */
        if (hdr->msg_type == MSG_REGISTER && node_id == 0) {
            node_id = hdr->node_id;
            printf("✔ Node zarejestrowany (id=%d)\n", node_id);
            continue;
        }

        /* ===== ASSIGN TASK ===== */
        if (hdr->msg_type == MSG_ASSIGN_TASK &&
            hdr->p_type == PAYLOAD_TASK) {

            TaskPayload *task =
                (TaskPayload *)(buffer + sizeof(ProtocolHeader));

            char *word =
                (char *)(buffer + sizeof(ProtocolHeader)
                         + sizeof(TaskPayload));

            uint16_t word_len = ntohs(task->word_len);

            TurtleState turtle;
            turtle.x   = ntohs(task->start_x);
            turtle.y   = ntohs(task->start_y);
            turtle.dir = task->direction;

            char fragment[MAX_WORD_FRAGMENT + 1];
            memcpy(fragment, word, word_len);
            fragment[word_len] = '\0';

            printf("📥 Fragment: \"%s\"\n", fragment);
            printf("➡ Start: (%d,%d) dir=%d\n",
                   turtle.x, turtle.y, turtle.dir);

            /* ===== rysowanie ===== */
            Canvas *canvas = canvas_create(CANVAS_W, CANVAS_H);
            canvas_clear(canvas);

            draw_turtle(canvas, fragment, &turtle);

            /* ===== encode canvas ===== */
            int enc_size = canvas_encoded_size(canvas);
            uint8_t *enc_buf = malloc(enc_size);
            canvas_encode(canvas, enc_buf);

            /* ===== wysy�anie ===== */
            uint8_t outbuf[2048];

            CanvasPayload payload;
            payload.end_x = htons(turtle.x);
            payload.end_y = htons(turtle.y);
            payload.direction = turtle.dir;
            payload.canvas_len = htons(enc_size);

            ProtocolHeader out_hdr = protocol_make_header(
                MSG_TASK_DONE,
                PAYLOAD_CANVAS,
                node_id,
                sizeof(CanvasPayload) + enc_size
            );
            out_hdr.payload_len = htons(out_hdr.payload_len);

            memcpy(outbuf, &out_hdr, sizeof(out_hdr));
            memcpy(outbuf + sizeof(out_hdr),
                   &payload, sizeof(payload));
            memcpy(outbuf + sizeof(out_hdr) + sizeof(payload),
                   enc_buf, enc_size);

            sendto(sockfd,
                   outbuf,
                   sizeof(out_hdr) + sizeof(payload) + enc_size,
                   0,
                   (struct sockaddr *)&server_addr,
                   sizeof(server_addr));

            printf("📤 Wysłano canvas + stan końcowy\n");

            free(enc_buf);
            canvas_destroy(canvas);
        }
    }

    close(sockfd);
    return 0;
}
