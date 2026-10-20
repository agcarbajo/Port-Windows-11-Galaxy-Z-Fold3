#include "q2q_stm_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef Q2Q_STM_REPLAY_TEST
#include "touch_capture.h"
#endif

int main(void)
{
    uint8_t frame[Q2Q_STM_EVENT_SIZE] = {0};
    uint8_t transfer[Q2Q_STM_MAX_FIFO_TRANSFER];
    Q2Q_STM_CONTACT contact;
    const uint8_t *events = NULL;
    size_t length;

    frame[0] = (uint8_t)((1u << 6) | (9u << 2));
    frame[1] = 0xAB;
    frame[2] = 0x12;
    frame[3] = 0xCD;
    frame[4] = 10;
    frame[5] = 8;
    frame[6] = (uint8_t)((1u << 6) | 37u);
    frame[7] = (uint8_t)((1u << 6) | 31u);
    assert(Q2qStmDecodeCoordinate(frame, sizeof(frame), &contact) == Q2Q_STM_COORDINATE);
    assert(contact.id == 9 && contact.action == 1);
    assert(contact.x == 0xABC && contact.y == 0x12D);
    assert(contact.pressure == 37 && contact.type == 5 && contact.remaining == 31);
    assert(contact.major == 10 && contact.minor == 8);

    frame[0] = (uint8_t)((2u << 6) | (9u << 2));
    assert(Q2qStmDecodeCoordinate(frame, sizeof(frame), &contact) == Q2Q_STM_COORDINATE);
    assert(contact.action == 2);
    frame[0] = (uint8_t)((3u << 6) | (9u << 2));
    assert(Q2qStmDecodeCoordinate(frame, sizeof(frame), &contact) == Q2Q_STM_COORDINATE);
    assert(contact.action == 3);

    frame[0] = 0x43;
    assert(Q2qStmDecodeCoordinate(frame, sizeof(frame), &contact) == Q2Q_STM_IGNORED);
    frame[0] = 0;
    assert(Q2qStmDecodeCoordinate(frame, sizeof(frame), &contact) == Q2Q_STM_IGNORED);
    frame[0] = (uint8_t)((1u << 6) | (10u << 2));
    assert(Q2qStmDecodeCoordinate(frame, sizeof(frame), &contact) == Q2Q_STM_INVALID);
    assert(Q2qStmDecodeCoordinate(frame, 15, &contact) == Q2Q_STM_INVALID);
    assert(Q2qStmDecodeCoordinate(NULL, 16, &contact) == Q2Q_STM_INVALID);
    assert(Q2qStmDecodeCoordinate(frame, 16, NULL) == Q2Q_STM_INVALID);

    length = Q2qStmBuildFifoRead(transfer, sizeof(transfer), 1);
    assert(length == 18 && transfer[0] == 0x87);
    for (size_t index = 1; index < length; ++index)
        assert(transfer[index] == 0);
    memset(transfer, 0xA5, sizeof(transfer));
    assert(Q2qStmGetFifoEvents(transfer, 18, 1, &events));
    assert(events == transfer + 2);
    assert(!Q2qStmGetFifoEvents(transfer, 17, 1, &events));
    assert(!Q2qStmGetFifoEvents(NULL, 18, 1, &events));

    assert(Q2qStmBuildFifoRead(transfer, sizeof(transfer), 31) == 498);
    assert(Q2qStmBuildFifoRead(transfer, sizeof(transfer), 32) == 0);
    assert(Q2qStmBuildFifoRead(transfer, sizeof(transfer), 0) == 0);
    assert(Q2qStmBuildFifoRead(transfer, 17, 1) == 0);
    assert(Q2qStmBuildFifoRead(NULL, sizeof(transfer), 1) == 0);
    {
        uint8_t report[Q2Q_STM_HID_REPORT_SIZE] = {0x54};
        report[61] = 1;
        report[1] = 7;
        report[3] = 0x34;
        report[4] = 0x02;
        assert(Q2qStmOrientReport(report, sizeof(report), 0, 1, 0));
        assert(report[5] == 0xff && report[6] == 0x0f);
        assert(report[3] == 0x34 && report[4] == 0x02 && report[1] == 7);
        assert(Q2qStmOrientReport(report, sizeof(report), 0, 1, 0));
        assert(report[5] == 0 && report[6] == 0);
        report[5] = 0x00;
        report[6] = 0x08;
        assert(Q2qStmOrientReport(report, sizeof(report), 0, 1, 0));
        assert(report[5] == 0xff && report[6] == 0x07);
        assert(Q2qStmOrientReport(report, sizeof(report), 0, 0, 0));
        assert(report[5] == 0xff && report[6] == 0x07);
        assert(!Q2qStmOrientReport(report, sizeof(report) - 1, 0, 1, 0));
        assert(!Q2qStmOrientReport(NULL, sizeof(report), 0, 1, 0));
        assert(Q2qStmOrientReport(report, sizeof(report), 1, 0, 0));
        assert(report[3] == 0xcb && report[4] == 0x0d);
        assert(report[5] == 0xff && report[6] == 0x07 && report[1] == 7);
        assert(Q2qStmOrientReport(report, sizeof(report), 1, 0, 0));
        assert(report[3] == 0x34 && report[4] == 0x02);
        report[3] = report[4] = 0;
        assert(Q2qStmOrientReport(report, sizeof(report), 1, 0, 0));
        assert(report[3] == 0xff && report[4] == 0x0f);
        assert(Q2qStmOrientReport(report, sizeof(report), 1, 0, 0));
        assert(report[3] == 0 && report[4] == 0);
        assert(!Q2qStmOrientReport(report, sizeof(report), 2, 0, 0));
    }
    {
        uint8_t report[Q2Q_STM_HID_REPORT_SIZE], expected[Q2Q_STM_HID_REPORT_SIZE], original[Q2Q_STM_HID_REPORT_SIZE];
        const uint16_t corners[4][2] = {{0, 0}, {0, 4095}, {4095, 0}, {4095, 4095}};
        memset(report, 0, sizeof(report));
        report[0] = 0x54;
        report[61] = 4;
        for (size_t index = 0; index < 4; ++index) {
            size_t offset = 1 + index * 6;
            report[offset] = index == 3 ? 4 : 7;
            report[offset + 1] = (uint8_t)index;
            report[offset + 2] = (uint8_t)corners[index][0];
            report[offset + 3] = (uint8_t)(corners[index][0] >> 8);
            report[offset + 4] = (uint8_t)corners[index][1];
            report[offset + 5] = (uint8_t)(corners[index][1] >> 8);
        }
        memcpy(original, report, sizeof(report));
        memcpy(expected, report, sizeof(report));
        for (size_t index = 0; index < 4; ++index) {
            size_t offset = 3 + index * 6;
            expected[offset] = original[offset + 2];
            expected[offset + 1] = original[offset + 3];
            expected[offset + 2] = original[offset];
            expected[offset + 3] = original[offset + 1];
        }
        assert(Q2qStmOrientReport(report, sizeof(report), 0, 0, 1));
        assert(memcmp(report, expected, sizeof(report)) == 0);
        assert(Q2qStmOrientReport(report, sizeof(report), 0, 0, 1));
        assert(memcmp(report, original, sizeof(report)) == 0);
        report[61] = 1;
        report[3] = 0x34; report[4] = 0x02;
        report[5] = 0x78; report[6] = 0x05;
        assert(Q2qStmOrientReport(report, sizeof(report), 1, 0, 1));
        assert(report[3] == 0x78 && report[4] == 0x05 && report[5] == 0xcb && report[6] == 0x0d);
        memcpy(original, report, sizeof(report));
        assert(!Q2qStmOrientReport(report, sizeof(report), 0, 0, 2));
        assert(memcmp(report, original, sizeof(report)) == 0);
        report[61] = 2;
        report[12] = 0x10;
        memcpy(original, report, sizeof(report));
        assert(!Q2qStmOrientReport(report, sizeof(report), 1, 0, 1));
        assert(memcmp(report, original, sizeof(report)) == 0);
    }
    {
        uint8_t command[3], echo[Q2Q_STM_EVENT_SIZE] = {0x43, 0x01, 0xa0, 0x00, 0x01};
        uint8_t identity[14] = {0x00, 0xfb, 0x20, 0x00, 0x00, 0x00, 0x36, 0x50, 0x01, 0x00, 0x00, 0x01, 0x24, 0x56};
        assert(Q2qStmBuildScanCommand(command, sizeof(command), 1) == 3);
        assert(command[0] == 0xa0 && command[1] == 0 && command[2] == 1);
        assert(Q2qStmScanEcho(echo, sizeof(echo), 1));
        assert(!Q2qStmScanEcho(echo, sizeof(echo), 0));
        echo[4] = 0;
        assert(Q2qStmScanEcho(echo, sizeof(echo), 0));
        assert(!Q2qStmScanEcho(echo, 15, 0));
        assert(!Q2qStmScanEcho(NULL, 16, 0));
        assert(Q2qStmBuildScanCommand(command, sizeof(command), 0) == 3 && command[2] == 0);
        assert(!Q2qStmBuildScanCommand(command, 2, 1));
        assert(!Q2qStmBuildScanCommand(command, 3, 2));
        assert(Q2qStmHardwareIdentity(identity, sizeof(identity)));
        identity[12] ^= 1;
        assert(!Q2qStmHardwareIdentity(identity, sizeof(identity)));
        assert(!Q2qStmHardwareIdentity(identity, 13));
        assert(!Q2qStmHardwareIdentity(NULL, 14));
    }
    {
        size_t remaining = 99;
        uint8_t full[Q2Q_STM_EVENT_SIZE] = {0};
        full[7] = 31;
        assert(Q2qStmFifoDrainCount(full, sizeof(full), 32, &remaining));
        assert(remaining == 31);
        assert(!Q2qStmFifoDrainCount(full, sizeof(full), 31, &remaining));
        assert(!Q2qStmFifoDrainCount(full, 15, 32, &remaining));
        assert(!Q2qStmFifoDrainCount(NULL, 16, 32, &remaining));
        assert(!Q2qStmFifoDrainCount(full, 16, 32, NULL));
        full[7] = 0;
        assert(Q2qStmFifoDrainCount(full, sizeof(full), 1, &remaining));
        assert(remaining == 0);
    }
    {
        Q2Q_STM_STATE state;
        uint8_t report[Q2Q_STM_HID_REPORT_SIZE];
        Q2qStmResetState(&state);
        memset(frame, 0, sizeof(frame));
        frame[0] = (1u << 6);
        frame[1] = 0xAB;
        frame[3] = 0xC0;
        assert(Q2qStmApplyEvent(&state, frame, 16) == Q2Q_STM_COORDINATE);
        assert(state.active == 1 && state.dirty);
        frame[0] = (uint8_t)((1u << 6) | (9u << 2));
        assert(Q2qStmApplyEvent(&state, frame, 16) == Q2Q_STM_COORDINATE);
        assert(Q2qStmBuildReport(&state, report, sizeof(report)) == 62);
        assert(report[61] == 2 && report[1] == 7 && report[2] == 0 && report[8] == 9);
        assert(report[3] == 0xBC && report[4] == 0x0A);
        Q2qStmCommitReport(&state);
        frame[0] = (uint8_t)((3u << 6) | (9u << 2));
        assert(Q2qStmApplyEvent(&state, frame, 16) == Q2Q_STM_COORDINATE);
        assert(state.released == (1u << 9));
        assert(Q2qStmBuildReport(&state, report, sizeof(report)) == 62);
        assert(report[61] == 2 && report[7] == 4 && report[8] == 9);
        assert(state.released != 0);
        Q2qStmCommitReport(&state);
        assert(Q2qStmBuildReport(&state, report, sizeof(report)) == 62 && report[61] == 1);
        frame[0] = 5;
        frame[1] = 1;
        assert(Q2qStmApplyEvent(&state, frame, 16) == Q2Q_STM_COORDINATE);
        assert(state.active == 0 && state.released == 1);
        assert(Q2qStmBuildReport(&state, report, sizeof(report)) == 62 && report[61] == 1 && report[1] == 0);
        assert(Q2qStmBuildReport(&state, report, 61) == 0);
    }
    {
        Q2Q_STM_STATE state;
        Q2Q_STM_REPORT_QUEUE queue;
        uint8_t report[Q2Q_STM_HID_REPORT_SIZE];
        Q2qStmResetState(&state);
        Q2qStmResetQueue(&queue);
        memset(frame, 0, sizeof(frame));
        frame[0] = 0x40;
        assert(Q2qStmQueueEvent(&state, &queue, frame, sizeof(frame)) == Q2Q_STM_COORDINATE);
        frame[0] = 0xc0;
        assert(Q2qStmQueueEvent(&state, &queue, frame, sizeof(frame)) == Q2Q_STM_COORDINATE);
        assert(queue.count == 2);
        assert(Q2qStmPeekReport(&queue, report, sizeof(report)) && report[1] == 7 && report[61] == 1);
        Q2qStmConsumeReport(&queue);
        assert(queue.delivered == 1);
        assert(Q2qStmPeekReport(&queue, report, sizeof(report)) && report[1] == 4 && report[61] == 1);
        Q2qStmConsumeReport(&queue);
        assert(queue.delivered == 0 && queue.count == 0);
        frame[0] = 0x40;
        for (size_t index = 0; index <= Q2Q_STM_REPORT_QUEUE_CAPACITY; ++index)
            assert(Q2qStmQueueEvent(&state, &queue, frame, sizeof(frame)) == Q2Q_STM_COORDINATE);
        assert(queue.overruns == 1 && queue.count == 2);
        assert(Q2qStmPeekReport(&queue, report, sizeof(report)) && report[1] == 0);
        Q2qStmConsumeReport(&queue);
        assert(Q2qStmPeekReport(&queue, report, sizeof(report)) && report[1] == 7);
        Q2qStmConsumeReport(&queue);
        Q2qStmCancelContacts(&state, &queue);
        assert(state.active == 0 && queue.count == 1);
        assert(Q2qStmPeekReport(&queue, report, sizeof(report)) && report[1] == 0);
        Q2qStmConsumeReport(&queue);
        assert(queue.delivered == 0);
        assert(!Q2qStmPeekReport(&queue, report, sizeof(report)));
        assert(!Q2qStmPeekReport(&queue, report, sizeof(report) - 1));
    }
    {
        Q2Q_STM_STATE state;
        uint8_t report[Q2Q_STM_HID_REPORT_SIZE];
        Q2qStmResetState(&state);
        memset(frame, 0, sizeof(frame));
        frame[0] = 0x40;
        assert(Q2qStmApplyEvent(&state, frame, 16) == Q2Q_STM_COORDINATE);
        frame[0] = 0x80;
        frame[6] = 0x40;
        frame[7] = 0x40;
        assert(Q2qStmApplyEvent(&state, frame, 16) == Q2Q_STM_COORDINATE);
        assert(Q2qStmBuildReport(&state, report, sizeof(report)) == 62 && report[1] == 3);
        frame[6] = frame[7] = 0;
        assert(Q2qStmApplyEvent(&state, frame, 16) == Q2Q_STM_COORDINATE);
        assert(Q2qStmBuildReport(&state, report, sizeof(report)) == 62 && report[1] == 3);
        frame[0] = 0xc0;
        assert(Q2qStmApplyEvent(&state, frame, 16) == Q2Q_STM_COORDINATE);
        assert(Q2qStmBuildReport(&state, report, sizeof(report)) == 62 && report[1] == 0);
        Q2qStmCommitReport(&state);
        frame[0] = 0x40;
        assert(Q2qStmApplyEvent(&state, frame, 16) == Q2Q_STM_COORDINATE);
        assert(Q2qStmBuildReport(&state, report, sizeof(report)) == 62 && report[1] == 7);
        frame[0] = 0xc0;
        assert(Q2qStmApplyEvent(&state, frame, 16) == Q2Q_STM_COORDINATE);
        assert(Q2qStmBuildReport(&state, report, sizeof(report)) == 62 && report[1] == 4);
    }
#ifdef Q2Q_STM_REPLAY_TEST
    {
        Q2Q_STM_STATE state;
        Q2Q_STM_REPORT_QUEUE queue;
        uint8_t report[Q2Q_STM_HID_REPORT_SIZE];
        Q2qStmResetState(&state);
        Q2qStmResetQueue(&queue);
        for (size_t index = 0; index < 64; ++index) {
            assert(Q2qStmQueueEvent(&state, &queue, Q2qCapturedFrames[index], 16) == Q2Q_STM_COORDINATE);
            assert(Q2qStmPeekReport(&queue, report, sizeof(report)) && report[61] == 1);
            assert(report[1] == (Q2qCapturedExpected[index][2] == 3 ? 4 : 7));
            assert(report[2] == 0);
            assert(((uint16_t)report[3] | ((uint16_t)report[4] << 8)) == Q2qCapturedExpected[index][0]);
            assert(((uint16_t)report[5] | ((uint16_t)report[6] << 8)) == Q2qCapturedExpected[index][1]);
            Q2qStmConsumeReport(&queue);
            assert(queue.count == 0);
        }
        puts("64 preserved real touch events replayed into native HID reports without hardware access");
    }
#endif
    puts("q2q STM packet, FIFO and HID state tests passed");
    return 0;
}
