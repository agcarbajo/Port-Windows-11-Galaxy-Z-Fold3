#ifndef Q2Q_STM_PROTOCOL_H
#define Q2Q_STM_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define Q2Q_STM_EVENT_SIZE 16u
#define Q2Q_STM_MAX_CONTACTS 10u
#define Q2Q_STM_MAX_FIFO_EVENTS 31u
#define Q2Q_STM_FIFO_PREFIX_SIZE 2u
#define Q2Q_STM_MAX_FIFO_TRANSFER (Q2Q_STM_FIFO_PREFIX_SIZE + Q2Q_STM_EVENT_SIZE * Q2Q_STM_MAX_FIFO_EVENTS)

typedef enum {
    Q2Q_STM_INVALID = -1,
    Q2Q_STM_IGNORED = 0,
    Q2Q_STM_COORDINATE = 1
} Q2Q_STM_DECODE_RESULT;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t id;
    uint8_t action;
    uint8_t type;
    uint8_t pressure;
    uint8_t major;
    uint8_t minor;
    uint8_t remaining;
} Q2Q_STM_CONTACT;

#define Q2Q_STM_HID_REPORT_SIZE 62u

typedef struct {
    Q2Q_STM_CONTACT contacts[Q2Q_STM_MAX_CONTACTS];
    uint16_t active;
    uint16_t released;
    uint16_t not_confident;
    uint8_t dirty;
} Q2Q_STM_STATE;

#define Q2Q_STM_REPORT_QUEUE_CAPACITY 64u

typedef struct {
    uint8_t reports[Q2Q_STM_REPORT_QUEUE_CAPACITY][Q2Q_STM_HID_REPORT_SIZE];
    size_t head, count;
    uint16_t delivered;
    uint32_t overruns;
} Q2Q_STM_REPORT_QUEUE;

void Q2qStmResetQueue(Q2Q_STM_REPORT_QUEUE *queue);
int Q2qStmQueueEvent(Q2Q_STM_STATE *state, Q2Q_STM_REPORT_QUEUE *queue, const uint8_t *frame, size_t length);
int Q2qStmPeekReport(const Q2Q_STM_REPORT_QUEUE *queue, uint8_t *report, size_t capacity);
void Q2qStmConsumeReport(Q2Q_STM_REPORT_QUEUE *queue);
void Q2qStmCancelContacts(Q2Q_STM_STATE *state, Q2Q_STM_REPORT_QUEUE *queue);
void Q2qStmResetState(Q2Q_STM_STATE *state);
int Q2qStmApplyEvent(Q2Q_STM_STATE *state, const uint8_t *frame, size_t length);
size_t Q2qStmBuildReport(const Q2Q_STM_STATE *state, uint8_t *report, size_t capacity);
int Q2qStmOrientReport(uint8_t *report, size_t length, uint8_t invert_x, uint8_t invert_y, uint8_t exchange_xy);
void Q2qStmCommitReport(Q2Q_STM_STATE *state);

Q2Q_STM_DECODE_RESULT Q2qStmDecodeCoordinate(const uint8_t *frame, size_t length, Q2Q_STM_CONTACT *contact);
size_t Q2qStmBuildFifoRead(uint8_t *transfer, size_t capacity, size_t event_count);
int Q2qStmGetFifoEvents(const uint8_t *response, size_t length, size_t event_count, const uint8_t **events);
int Q2qStmFifoDrainCount(const uint8_t *first, size_t length, size_t capacity_events, size_t *remaining);
size_t Q2qStmBuildScanCommand(uint8_t *command, size_t capacity, uint8_t mode);
int Q2qStmScanEcho(const uint8_t *event, size_t length, uint8_t mode);
int Q2qStmHardwareIdentity(const uint8_t *response, size_t length);

#endif
