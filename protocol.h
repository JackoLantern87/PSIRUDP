#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* =========================================================
 *  Konfiguracja ogólna
 * ========================================================= */

#define PROTOCOL_VERSION 1

#define SERVER_PORT 8080
#define MAX_NODES 4

/* Maksymalne rozmiary danych (bezpieczne dla UDP) */
#define MAX_WORD_FRAGMENT   256
#define MAX_CANVAS_PAYLOAD  1024   /* canvas wysyłany w kawałkach */

/* =========================================================
 *  Typy wiadomości
 * ========================================================= */

typedef enum {
    MSG_REGISTER = 1,      /* Node  -> Server : żądanie rejestracji */
    MSG_ASSIGN_TASK,       /* Server-> Node   : przydział zadania */
    MSG_CANVAS_DATA,       /* Node  -> Server : fragment canvasu */
    MSG_TASK_DONE          /* Node  -> Server : koniec rysowania */
} MessageType;

/* =========================================================
 *  Typy payloadów
 * ========================================================= */

typedef enum {
    PAYLOAD_EMPTY = 0,
    PAYLOAD_TASK,          /* stan żółwia + fragment słowa */
    PAYLOAD_CANVAS         /* zakodowany fragment canvasu */
} PayloadType;

/* =========================================================
 *  Nagłówek protokołu
 * ========================================================= */

typedef struct {
    uint8_t  version;      /* wersja protokołu */
    uint8_t  msg_type;     /* MessageType */
    uint8_t  node_id;      /* ID węzła (nadane przez serwer) */
    uint8_t  p_type;       /* PayloadType */
    uint16_t payload_len;  /* długość payloadu w bajtach */
} __attribute__((packed)) ProtocolHeader;

/* =========================================================
 *  Payload: zadanie dla węzła
 * ========================================================= */

typedef struct {
    int16_t start_x;       /* pozycja startowa żółwia */
    int16_t start_y;
    uint8_t direction;    /* kierunek początkowy (0–7) */
    uint16_t word_len;    /* długość fragmentu słowa */
    /* po tej strukturze idą znaki słowa (char[word_len]) */
} __attribute__((packed)) TaskPayload;

/* =========================================================
 *  Payload: dane canvasu
 * ========================================================= */

typedef struct {
    uint16_t offset;      /* offset w obrazie (do składania) */
    uint16_t data_len;    /* liczba bajtów danych */
    /* po tej strukturze idą dane canvasu */
} __attribute__((packed)) CanvasPayload;

/* =========================================================
 *  Pomocnicze funkcje (opcjonalne, inline)
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
