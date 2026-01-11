#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.h"

#define SERVER_IP   "192.168.56.101"   // IP serwera
#define SERVER_PORT 8080

int main(void) {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addrlen = sizeof(server_addr);
    uint8_t buffer[1500];
    uint8_t node_id = 0;

    /* =========================================================
     * Socket UDP
     * ========================================================= */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    /* =========================================================
     * Rejestracja w serwerze
     * ========================================================= */
    ProtocolHeader hdr = protocol_make_header(
        MSG_REGISTER,
        PAYLOAD_EMPTY,
        0,
        0
    );
    hdr.payload_len = htons(0);

    sendto(sockfd, &hdr, sizeof(hdr), 0,
           (struct sockaddr *)&server_addr, sizeof(server_addr));

    printf("🔹 Node: wysłano MSG_REGISTER\n");

    /* =========================================================
     * Pętla główna
     * ========================================================= */
    while (1) {
        ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&server_addr,
                               &addrlen);
        if (len < (ssize_t)sizeof(ProtocolHeader))
            continue;

        ProtocolHeader *recv_hdr = (ProtocolHeader *)buffer;

        if (recv_hdr->version != PROTOCOL_VERSION)
            continue;

        /* =====================================================
         * Potwierdzenie rejestracji
         * ===================================================== */
        if (recv_hdr->msg_type == MSG_REGISTER && node_id == 0) {
            node_id = recv_hdr->node_id;
            printf("✔ Node zarejestrowany, node_id=%d\n", node_id);
            continue;
        }

        /* =====================================================
         * Odbiór zadania
         * ===================================================== */
        if (recv_hdr->msg_type == MSG_ASSIGN_TASK) {
            uint16_t payload_len = ntohs(recv_hdr->payload_len);

            if (recv_hdr->p_type != PAYLOAD_TASK || payload_len == 0)
                continue;

            TaskPayload *task =
                (TaskPayload *)(buffer + sizeof(ProtocolHeader));

            char *word_buf =
                (char *)(buffer + sizeof(ProtocolHeader) + sizeof(TaskPayload));

            uint16_t word_len = ntohs(task->word_len);

            if (word_len >= 255)
                continue;

            char word[256];
            memcpy(word, word_buf, word_len);
            word[word_len] = '\0';

            printf("📥 Otrzymano słowo: \"%s\"\n", word);

            /* =================================================
             * Modyfikacja słowa:
             * każdy node dopisuje swoją literę
             * ================================================= */
            if (word_len < sizeof(word) - 1) {
                char c = 'A' + node_id - 1;   // node 1 -> A, 2 -> B, ...
                word[word_len] = c;
                word[word_len + 1] = '\0';
                word_len++;
            }

            printf("🛠 Po modyfikacji: \"%s\"\n", word);

            /* =================================================
             * Odesłanie MSG_TASK_DONE
             * ================================================= */
            uint8_t send_buf[1500];

            ProtocolHeader out_hdr = protocol_make_header(
                MSG_TASK_DONE,
                PAYLOAD_TASK,
                node_id,
                word_len
            );
            out_hdr.payload_len = htons(word_len);

            memcpy(send_buf, &out_hdr, sizeof(ProtocolHeader));
            memcpy(send_buf + sizeof(ProtocolHeader), word, word_len);

            sendto(sockfd,
                   send_buf,
                   sizeof(ProtocolHeader) + word_len,
                   0,
                   (struct sockaddr *)&server_addr,
                   sizeof(server_addr));

            printf("📤 Wysłano MSG_TASK_DONE: \"%s\"\n", word);
        }
    }

    close(sockfd);
    return 0;
}
