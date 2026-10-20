#include "q2q_stm_protocol.h"

void Q2qStmResetQueue(Q2Q_STM_REPORT_QUEUE *queue)
{
    if (queue)
        for (size_t index = 0; index < sizeof(*queue); ++index)
            ((uint8_t *)queue)[index] = 0;
}

static int Q2qStmQueueValid(const Q2Q_STM_REPORT_QUEUE *queue)
{
    return queue && queue->head < Q2Q_STM_REPORT_QUEUE_CAPACITY && queue->count <= Q2Q_STM_REPORT_QUEUE_CAPACITY;
}

static void Q2qStmAppendReport(Q2Q_STM_REPORT_QUEUE *queue, const Q2Q_STM_STATE *state)
{
    size_t index = (queue->head + queue->count) % Q2Q_STM_REPORT_QUEUE_CAPACITY;
    Q2qStmBuildReport(state, queue->reports[index], Q2Q_STM_HID_REPORT_SIZE);
    ++queue->count;
}

int Q2qStmQueueEvent(Q2Q_STM_STATE *state, Q2Q_STM_REPORT_QUEUE *queue, const uint8_t *frame, size_t length)
{
    int result;
    if (!state || !Q2qStmQueueValid(queue))
        return Q2Q_STM_INVALID;
    result = Q2qStmApplyEvent(state, frame, length);
    if (result != Q2Q_STM_COORDINATE || !state->dirty)
        return result;
    if (queue->count == Q2Q_STM_REPORT_QUEUE_CAPACITY) {
        Q2Q_STM_STATE cancel = *state;
        cancel.released |= (uint16_t)(cancel.active | queue->delivered);
        cancel.not_confident |= cancel.released;
        cancel.active = 0;
        queue->head = queue->count = 0;
        ++queue->overruns;
        Q2qStmAppendReport(queue, &cancel);
    }
    Q2qStmAppendReport(queue, state);
    Q2qStmCommitReport(state);
    return result;
}

int Q2qStmPeekReport(const Q2Q_STM_REPORT_QUEUE *queue, uint8_t *report, size_t capacity)
{
    if (!Q2qStmQueueValid(queue) || !queue->count || !report || capacity < Q2Q_STM_HID_REPORT_SIZE)
        return 0;
    for (size_t index = 0; index < Q2Q_STM_HID_REPORT_SIZE; ++index)
        report[index] = queue->reports[queue->head][index];
    return 1;
}

void Q2qStmConsumeReport(Q2Q_STM_REPORT_QUEUE *queue)
{
    uint8_t *report;
    if (!Q2qStmQueueValid(queue) || !queue->count)
        return;
    report = queue->reports[queue->head];
    queue->delivered = 0;
    for (size_t index = 0; index < report[Q2Q_STM_HID_REPORT_SIZE - 1] && index < Q2Q_STM_MAX_CONTACTS; ++index)
        if ((report[1 + index * 6] & 1u) && report[2 + index * 6] < Q2Q_STM_MAX_CONTACTS)
            queue->delivered |= (uint16_t)(1u << report[2 + index * 6]);
    queue->head = (queue->head + 1) % Q2Q_STM_REPORT_QUEUE_CAPACITY;
    --queue->count;
}

void Q2qStmCancelContacts(Q2Q_STM_STATE *state, Q2Q_STM_REPORT_QUEUE *queue)
{
    if (!state || !Q2qStmQueueValid(queue))
        return;
    state->released |= (uint16_t)(state->active | queue->delivered);
    state->not_confident |= state->released;
    state->active = 0;
    state->dirty = 1;
    queue->head = queue->count = 0;
    Q2qStmAppendReport(queue, state);
    Q2qStmCommitReport(state);
}

void Q2qStmResetState(Q2Q_STM_STATE *state)
{
    if (state) {
        for (size_t index = 0; index < sizeof(*state); ++index)
            ((uint8_t *)state)[index] = 0;
    }
}

int Q2qStmApplyEvent(Q2Q_STM_STATE *state, const uint8_t *frame, size_t length)
{
    Q2Q_STM_CONTACT contact;
    uint16_t mask;
    Q2Q_STM_DECODE_RESULT result;
    if (!state || !frame || length != Q2Q_STM_EVENT_SIZE)
        return Q2Q_STM_INVALID;
    if ((frame[0] & 3u) == 1u && ((((frame[0] >> 2) & 15u) == 1u && frame[1] == 1u) || (((frame[0] >> 2) & 15u) == 2u && frame[1] == 0u))) {
        state->released |= state->active;
        state->not_confident |= state->released;
        state->active = 0;
        state->dirty = 1;
        return Q2Q_STM_COORDINATE;
    }
    result = Q2qStmDecodeCoordinate(frame, length, &contact);
    if (result != Q2Q_STM_COORDINATE)
        return result;
    if (contact.type != 0 && contact.type != 3 && contact.type != 5 && contact.type != 6)
        return Q2Q_STM_IGNORED;
    mask = (uint16_t)(1u << contact.id);
    if (contact.action == 1)
        state->not_confident &= (uint16_t)~mask;
    if (contact.type == 5)
        state->not_confident |= mask;
    state->contacts[contact.id] = contact;
    if (contact.action == 3) {
        state->active &= (uint16_t)~mask;
        state->released |= mask;
    } else {
        state->active |= mask;
        state->released &= (uint16_t)~mask;
    }
    state->dirty = 1;
    return Q2Q_STM_COORDINATE;
}

size_t Q2qStmBuildReport(const Q2Q_STM_STATE *state, uint8_t *report, size_t capacity)
{
    uint8_t count = 0;
    if (!state || !report || capacity < Q2Q_STM_HID_REPORT_SIZE)
        return 0;
    for (size_t index = 0; index < Q2Q_STM_HID_REPORT_SIZE; ++index)
        report[index] = 0;
    report[0] = 0x54;
    for (size_t id = 0; id < Q2Q_STM_MAX_CONTACTS; ++id) {
        uint16_t mask = (uint16_t)(1u << id);
        const Q2Q_STM_CONTACT *contact = &state->contacts[id];
        size_t offset;
        if (!((state->active | state->released) & mask))
            continue;
        offset = 1u + (size_t)count * 6u;
        report[offset] = (uint8_t)(((state->active & mask) ? 3u : 0u) | ((state->not_confident & mask) ? 0u : 4u));
        report[offset + 1] = (uint8_t)id;
        report[offset + 2] = (uint8_t)contact->x;
        report[offset + 3] = (uint8_t)(contact->x >> 8);
        report[offset + 4] = (uint8_t)contact->y;
        report[offset + 5] = (uint8_t)(contact->y >> 8);
        ++count;
    }
    report[Q2Q_STM_HID_REPORT_SIZE - 1] = count;
    return Q2Q_STM_HID_REPORT_SIZE;
}

int Q2qStmOrientReport(uint8_t *report, size_t length, uint8_t invert_x, uint8_t invert_y, uint8_t exchange_xy)
{
    if (!report || length != Q2Q_STM_HID_REPORT_SIZE || report[0] != 0x54 || report[length - 1] > Q2Q_STM_MAX_CONTACTS || invert_x > 1 || invert_y > 1 || exchange_xy > 1)
        return 0;
    for (size_t index = 0; index < report[length - 1]; ++index)
        for (size_t axis = 0; axis < 2; ++axis) {
            size_t offset = 3 + index * 6 + axis * 2;
            if ((report[offset] | ((uint16_t)report[offset + 1] << 8)) > 4095)
                return 0;
        }
    for (size_t index = 0; index < report[length - 1]; ++index) {
        for (size_t axis = 0; axis < 2; ++axis) {
            size_t offset = 3 + index * 6 + axis * 2;
            if (axis ? invert_y : invert_x) {
                uint16_t value = (uint16_t)(4095 - (report[offset] | ((uint16_t)report[offset + 1] << 8)));
                report[offset] = (uint8_t)value;
                report[offset + 1] = (uint8_t)(value >> 8);
            }
        }
        if (exchange_xy)
            for (size_t byte = 0; byte < 2; ++byte) {
                size_t offset = 3 + index * 6 + byte;
                uint8_t value = report[offset];
                report[offset] = report[offset + 2];
                report[offset + 2] = value;
            }
    }
    return 1;
}

void Q2qStmCommitReport(Q2Q_STM_STATE *state)
{
    if (state) {
        state->released = 0;
        state->dirty = 0;
    }
}

Q2Q_STM_DECODE_RESULT Q2qStmDecodeCoordinate(const uint8_t *frame, size_t length, Q2Q_STM_CONTACT *contact)
{
    uint8_t id;
    uint8_t action;
    if (!frame || !contact || length != Q2Q_STM_EVENT_SIZE)
        return Q2Q_STM_INVALID;
    if ((frame[0] & 3u) != 0)
        return Q2Q_STM_IGNORED;
    id = (frame[0] >> 2) & 15u;
    action = frame[0] >> 6;
    if (action == 0)
        return Q2Q_STM_IGNORED;
    if (id >= Q2Q_STM_MAX_CONTACTS)
        return Q2Q_STM_INVALID;
    contact->x = (uint16_t)((frame[1] << 4) | (frame[3] >> 4));
    contact->y = (uint16_t)((frame[2] << 4) | (frame[3] & 15u));
    contact->id = id;
    contact->action = action;
    contact->type = (uint8_t)(((frame[6] >> 6) << 2) | (frame[7] >> 6));
    contact->pressure = frame[6] & 63u;
    contact->major = frame[4];
    contact->minor = frame[5];
    contact->remaining = frame[7] & 31u;
    return Q2Q_STM_COORDINATE;
}

size_t Q2qStmBuildFifoRead(uint8_t *transfer, size_t capacity, size_t event_count)
{
    size_t length;
    if (!transfer || event_count == 0 || event_count > Q2Q_STM_MAX_FIFO_EVENTS)
        return 0;
    length = Q2Q_STM_FIFO_PREFIX_SIZE + event_count * Q2Q_STM_EVENT_SIZE;
    if (capacity < length)
        return 0;
    for (size_t index = 0; index < length; ++index)
        transfer[index] = 0;
    transfer[0] = 0x87;
    return length;
}

size_t Q2qStmBuildScanCommand(uint8_t *command, size_t capacity, uint8_t mode)
{
    if (!command || capacity < 3 || mode > 1)
        return 0;
    command[0] = 0xa0;
    command[1] = 0;
    command[2] = mode;
    return 3;
}

int Q2qStmScanEcho(const uint8_t *event, size_t length, uint8_t mode)
{
    return event && length == Q2Q_STM_EVENT_SIZE && mode <= 1 && event[0] == 0x43 && event[1] == 1 && event[2] == 0xa0 && event[3] == 0 && event[4] == mode;
}

int Q2qStmHardwareIdentity(const uint8_t *response, size_t length)
{
    const uint8_t expected[] = {0x00, 0xfb, 0x20, 0x00, 0x00, 0x00, 0x36, 0x50, 0x01, 0x00, 0x00, 0x01, 0x24, 0x56};
    if (!response || length != sizeof(expected))
        return 0;
    for (size_t index = 0; index < sizeof(expected); ++index)
        if (response[index] != expected[index])
            return 0;
    return 1;
}

int Q2qStmFifoDrainCount(const uint8_t *first, size_t length, size_t capacity_events, size_t *remaining)
{
    size_t count;
    if (!first || !remaining || length != Q2Q_STM_EVENT_SIZE)
        return 0;
    count = first[7] & 31u;
    if (count + 1 > capacity_events)
        return 0;
    *remaining = count;
    return 1;
}

int Q2qStmGetFifoEvents(const uint8_t *response, size_t length, size_t event_count, const uint8_t **events)
{
    if (!response || !events || event_count == 0 || event_count > Q2Q_STM_MAX_FIFO_EVENTS || length != Q2Q_STM_FIFO_PREFIX_SIZE + event_count * Q2Q_STM_EVENT_SIZE)
        return 0;
    *events = response + Q2Q_STM_FIFO_PREFIX_SIZE;
    return 1;
}
