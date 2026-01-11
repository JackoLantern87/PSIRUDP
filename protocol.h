#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* =========================================================
 *  Konfiguracja
 * ========================================================= */

#define PROTOCOL_VERSION 1

#define SERVER_PORT 8080
#define MAX_NODES 2

/* Bezpieczne rozmiary dla UDP */
#define MAX_WORD_FRAGMENT   256
#define MAX_CANVAS_PAYLOAD  2048

/* =========================================================
 *  Typy wiadomości
 * ========================================================= */

typedef enum {
    MSG_REGISTER = 1,      /* Node  -> Server */
    MSG_ASSIGN_TASK,       /* Server-> Node  */
    MSG_TASK_DONE          /* Node  -> Server */
} MessageType;

/* =========================================================
 *  Typy payloadów
 * ========================================================= */

typedef enum {
    PAYLOAD_EMPTY = 0,
    PAYLOAD_TASK,
    PAYLOAD_CANVAS
} PayloadType;

/* =========================================================
 *  Nagłówek protokołu
 * ========================================================= */

typedef struct {
    uint8_t  version;      /* PROTOCOL_VERSION */
    uint8_t  msg_type;     /* MessageType */
    uint8_t  node_id;      /* ID węzła */
    uint8_t  p_type;       /* PayloadType */
    uint16_t payload_len;  /* długość payloadu */
} __attribute__((packed)) ProtocolHeader;

/* =========================================================
 *  Payload: zadanie dla node
 * =========================================================
 *  Po strukturze idzie:
 *      char word[word_len]
 */

typedef struct {
    int16_t  start_x;
    int16_t  start_y;
    uint8_t  direction;    /* 0–7 */
    uint16_t word_len;
} __attribute__((packed)) TaskPayload;

/* =========================================================
 *  Payload: wynik rysowania (canvas)
 * =========================================================
 *  Po strukturze idzie:
 *      uint8_t canvas_data[canvas_len]
 */

typedef struct {
    int16_t  end_x;
    int16_t  end_y;
    uint8_t  direction;
    uint16_t canvas_len;
} __attribute__((packed)) CanvasPayload;

/* =========================================================
 *  Helper
 * ========================================================= */

static inline ProtocolHeader protocol_make_header(
    MessageType msg,
    PayloadType ptype,
    uint8_t node_id,
    uint16_t len
) {
    ProtocolHeader h;
    h.version = PROTOCOL_VERSION;
    h.msg_type = msg;
    h.node_id = node_id;
    h.p_type = ptype;
    h.payload_len = len;
    return h;
}

#endif /* PROTOCOL_H */
