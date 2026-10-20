#include "vhidmini.h"

void Q2qSpiRecordStatus(PDEVICE_CONTEXT device, NTSTATUS status)
{
    WDFKEY key;
    UNICODE_STRING name;
    if (device->ConnectionType != CM_RESOURCE_CONNECTION_TYPE_SERIAL_SPI)
        return;
    if (!NT_SUCCESS(WdfDeviceOpenRegistryKey(device->Device, PLUGPLAY_REGKEY_DEVICE, KEY_SET_VALUE, WDF_NO_OBJECT_ATTRIBUTES, &key)))
        return;
    RtlInitUnicodeString(&name, L"Q2qSpiLastStatus");
    WdfRegistryAssignULong(key, &name, (ULONG)status);
    RtlInitUnicodeString(&name, L"Q2qSpiFrames");
    WdfRegistryAssignULong(key, &name, device->SpiFrames);
    RtlInitUnicodeString(&name, L"Q2qSpiScanRestoreNeeded");
    WdfRegistryAssignULong(key, &name, device->SpiScanRestoreNeeded);
    RtlInitUnicodeString(&name, L"Q2qSpiPolling");
    WdfRegistryAssignULong(key, &name, (ULONG)InterlockedCompareExchange(&device->SpiPolling, 0, 0));
    RtlInitUnicodeString(&name, L"Q2qSpiPolls");
    WdfRegistryAssignULong(key, &name, device->SpiPolls);
    RtlInitUnicodeString(&name, L"Q2qSpiReportOverruns");
    WdfRegistryAssignULong(key, &name, device->InputReports.overruns);
    RtlInitUnicodeString(&name, L"Q2qSpiInvertY");
    WdfRegistryAssignULong(key, &name, device->InvertY);
    RtlInitUnicodeString(&name, L"Q2qSpiInvertX");
    WdfRegistryAssignULong(key, &name, device->InvertX);
    RtlInitUnicodeString(&name, L"Q2qSpiExchangeXY");
    WdfRegistryAssignULong(key, &name, device->ExchangeXY);
    WdfRegistryClose(key);
}

static NTSTATUS Q2qSpiExchange(PDEVICE_CONTEXT device, UCHAR *tx, UCHAR *rx, size_t length)
{
    SPB_TRANSFER_LIST_AND_ENTRIES(2) sequence;
    WDF_MEMORY_DESCRIPTOR descriptor;
    WDF_REQUEST_SEND_OPTIONS options;
    ULONG_PTR transferred = 0;
    NTSTATUS status;
    if (!device->SpbController || !tx || !rx || !length || length > Q2Q_STM_MAX_FIFO_TRANSFER)
        return STATUS_INVALID_PARAMETER;
    SPB_TRANSFER_LIST_INIT(&sequence.List, 2);
    sequence.List.Transfers[0] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionToDevice, 0, tx, (ULONG)length);
    sequence.List.Transfers[1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionFromDevice, 0, rx, (ULONG)length);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&descriptor, &sequence, sizeof(sequence));
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    options.Timeout = WDF_REL_TIMEOUT_IN_MS(250);
    status = WdfIoTargetSendIoctlSynchronously(device->SpbController, NULL, IOCTL_SPB_FULL_DUPLEX, &descriptor, NULL, &options, &transferred);
    return NT_SUCCESS(status) && transferred != length * 2 ? STATUS_DEVICE_DATA_ERROR : status;
}

static NTSTATUS Q2qSpiReadFifo(PDEVICE_CONTEXT device, UCHAR *buffer, size_t count)
{
    UCHAR tx[Q2Q_STM_MAX_FIFO_TRANSFER], rx[Q2Q_STM_MAX_FIFO_TRANSFER] = {0};
    const uint8_t *events;
    size_t length = Q2qStmBuildFifoRead(tx, sizeof(tx), count);
    NTSTATUS status;
    if (!length || !buffer)
        return STATUS_INVALID_PARAMETER;
    status = Q2qSpiExchange(device, tx, rx, length);
    if (!NT_SUCCESS(status))
        return status;
    if (!Q2qStmGetFifoEvents(rx, length, count, &events))
        return STATUS_DEVICE_DATA_ERROR;
    RtlCopyMemory(buffer, events, count * Q2Q_STM_EVENT_SIZE);
    return STATUS_SUCCESS;
}

static NTSTATUS Q2qSpiCheckIdentity(PDEVICE_CONTEXT device)
{
    UCHAR tx[14] = {0xfb, 0x20}, rx[14] = {0};
    NTSTATUS status = Q2qSpiExchange(device, tx, rx, sizeof(tx));
    return NT_SUCCESS(status) && !Q2qStmHardwareIdentity(rx, sizeof(rx)) ? STATUS_DEVICE_CONFIGURATION_ERROR : status;
}

static NTSTATUS Q2qSpiSetScan(PDEVICE_CONTEXT device, UCHAR mode)
{
    UCHAR command[3], event[Q2Q_STM_EVENT_SIZE];
    ULONGLONG deadline;
    if (Q2qStmBuildScanCommand(command, sizeof(command), mode) != sizeof(command))
        return STATUS_INVALID_PARAMETER;
    WDF_MEMORY_DESCRIPTOR descriptor;
    WDF_REQUEST_SEND_OPTIONS options;
    ULONG_PTR written = 0;
    NTSTATUS status;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&descriptor, command, sizeof(command));
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    options.Timeout = WDF_REL_TIMEOUT_IN_MS(250);
    status = WdfIoTargetSendWriteSynchronously(device->SpbController, NULL, &descriptor, NULL, &options, &written);
    if (!NT_SUCCESS(status))
        return status;
    if (written != sizeof(command))
        return STATUS_DEVICE_DATA_ERROR;
    deadline = KeQueryInterruptTime() + 20000000ULL;
    for (size_t attempt = 0; attempt < 64 && KeQueryInterruptTime() < deadline; ++attempt) {
        msleep(20);
        status = Q2qSpiReadFifo(device, event, 1);
        if (!NT_SUCCESS(status))
            return status;
        if (Q2qStmScanEcho(event, sizeof(event), mode))
            return STATUS_SUCCESS;
    }
    return STATUS_IO_TIMEOUT;
}

void Q2qSpiDeliver(PDEVICE_CONTEXT device)
{
    UCHAR report[Q2Q_STM_HID_REPORT_SIZE];
    WDFREQUEST request = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    for (size_t delivered = 0; delivered < Q2Q_STM_REPORT_QUEUE_CAPACITY; ++delivered) {
        request = NULL;
        WdfWaitLockAcquire(device->StateLock, NULL);
        if (Q2qStmPeekReport(&device->InputReports, report, sizeof(report)) && NT_SUCCESS(WdfIoQueueRetrieveNextRequest(device->ManualQueue, &request))) {
            status = Q2qStmOrientReport(report, sizeof(report), device->InvertX, device->InvertY, device->ExchangeXY) ? RequestCopyFromBuffer(request, report, sizeof(report)) : STATUS_DEVICE_DATA_ERROR;
            if (NT_SUCCESS(status))
                Q2qStmConsumeReport(&device->InputReports);
        }
        WdfWaitLockRelease(device->StateLock);
        if (!request)
            break;
        WdfRequestComplete(request, status);
    }
}

static VOID Q2qSpiPollTimer(WDFTIMER timer)
{
    PDEVICE_CONTEXT device = GetDeviceContext((WDFDEVICE)WdfTimerGetParentObject(timer));
    if (!InterlockedCompareExchange(&device->SpiPolling, 0, 0))
        return;
    ++device->SpiPolls;
    if (Q2qSpiInterrupt(device))
        device->SpiPollFailures = 0;
    else if (++device->SpiPollFailures >= 3 || !InterlockedCompareExchange(&device->SpiPolling, 0, 0)) {
        InterlockedExchange(&device->SpiPolling, 0);
        WdfDeviceSetFailed(device->Device, WdfDeviceFailedNoRestart);
    }
    if ((device->SpiPolls & 63u) == 0)
        Q2qSpiRecordStatus(device, device->SpiLastStatus);
    if (InterlockedCompareExchange(&device->SpiPolling, 0, 0))
        WdfTimerStart(device->SpiTimer, WDF_REL_TIMEOUT_IN_MS(8));
}

NTSTATUS Q2qSpiInitialize(PDEVICE_CONTEXT device)
{
    WDF_TIMER_CONFIG config;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFKEY key;
    ULONG invert_y = 0, invert_x = 0, exchange_xy = 0;
    NTSTATUS status = WdfDeviceOpenRegistryKey(device->Device, PLUGPLAY_REGKEY_DEVICE, KEY_READ, WDF_NO_OBJECT_ATTRIBUTES, &key);
    if (!NT_SUCCESS(status))
        return status;
    UNICODE_STRING name;
    RtlInitUnicodeString(&name, L"YRevert");
    status = WdfRegistryQueryULong(key, &name, &invert_y);
    if (NT_SUCCESS(status)) {
        RtlInitUnicodeString(&name, L"XRevert");
        status = WdfRegistryQueryULong(key, &name, &invert_x);
    }
    if (NT_SUCCESS(status)) {
        RtlInitUnicodeString(&name, L"XYExchange");
        status = WdfRegistryQueryULong(key, &name, &exchange_xy);
    }
    WdfRegistryClose(key);
    if (!NT_SUCCESS(status))
        return status;
    if (invert_y > 1 || invert_x > 1 || exchange_xy > 1)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    device->InvertY = (BOOLEAN)invert_y;
    device->InvertX = (BOOLEAN)invert_x;
    device->ExchangeXY = (BOOLEAN)exchange_xy;
    Q2qStmResetState(&device->TouchState);
    Q2qStmResetQueue(&device->InputReports);
    device->SpiLastStatus = STATUS_DEVICE_NOT_READY;
    WDF_TIMER_CONFIG_INIT(&config, Q2qSpiPollTimer);
    config.AutomaticSerialization = FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device->Device;
    attributes.ExecutionLevel = WdfExecutionLevelPassive;
    return WdfTimerCreate(&config, &attributes, &device->SpiTimer);
}

NTSTATUS Q2qSpiStop(PDEVICE_CONTEXT device)
{
    NTSTATUS status;
    InterlockedExchange(&device->SpiPolling, 0);
    if (device->SpiTimer)
        WdfTimerStop(device->SpiTimer, TRUE);
    WdfWaitLockAcquire(device->StateLock, NULL);
    Q2qStmCancelContacts(&device->TouchState, &device->InputReports);
    WdfWaitLockRelease(device->StateLock);
    Q2qSpiDeliver(device);
    if (!device->SpiScanRestoreNeeded)
        return STATUS_SUCCESS;
    status = Q2qSpiSetScan(device, 0);
    if (NT_SUCCESS(status))
        device->SpiScanRestoreNeeded = FALSE;
    return status;
}

NTSTATUS Q2qSpiStart(PDEVICE_CONTEXT device)
{
    NTSTATUS status;
    WdfWaitLockAcquire(device->StateLock, NULL);
    Q2qStmCancelContacts(&device->TouchState, &device->InputReports);
    WdfWaitLockRelease(device->StateLock);
    device->SpiFrames = 0;
    device->SpiPolls = 0;
    device->SpiPollFailures = 0;
    device->DiagnosticRecorded = FALSE;
    if (!device->SpiTimer)
        return STATUS_INVALID_DEVICE_STATE;
    status = Q2qSpiCheckIdentity(device);
    if (!NT_SUCCESS(status))
        return status;
    device->SpiScanRestoreNeeded = TRUE;
    status = Q2qSpiSetScan(device, 1);
    if (!NT_SUCCESS(status)) {
        Q2qSpiRecordStatus(device, Q2qSpiStop(device));
        return status;
    }
    device->SpiLastStatus = STATUS_SUCCESS;
    InterlockedExchange(&device->SpiPolling, 1);
    WdfTimerStart(device->SpiTimer, WDF_REL_TIMEOUT_IN_MS(8));
    return STATUS_SUCCESS;
}

BOOLEAN Q2qSpiInterrupt(PDEVICE_CONTEXT device)
{
    UCHAR frames[Q2Q_STM_EVENT_SIZE * (Q2Q_STM_MAX_FIFO_EVENTS + 1)];
    size_t count;
    NTSTATUS status = Q2qSpiReadFifo(device, frames, 1);
    if (!NT_SUCCESS(status))
        goto Failed;
    if (!Q2qStmFifoDrainCount(frames, Q2Q_STM_EVENT_SIZE, sizeof(frames) / Q2Q_STM_EVENT_SIZE, &count)) {
        status = STATUS_DEVICE_DATA_ERROR;
        goto Failed;
    }
    if (count) {
        status = Q2qSpiReadFifo(device, frames + Q2Q_STM_EVENT_SIZE, count);
        if (!NT_SUCCESS(status))
            goto Failed;
    }
    ++count;
    WdfWaitLockAcquire(device->StateLock, NULL);
    for (size_t index = 0; index < count; ++index) {
        const UCHAR *event = frames + index * Q2Q_STM_EVENT_SIZE;
        if ((event[0] & 3u) == 1u && ((event[0] >> 2) & 15u) == 2u && event[1] == 0) {
            InterlockedExchange(&device->SpiPolling, 0);
            status = STATUS_DEVICE_NOT_READY;
            break;
        }
        if (Q2qStmQueueEvent(&device->TouchState, &device->InputReports, event, Q2Q_STM_EVENT_SIZE) == Q2Q_STM_INVALID) {
            status = STATUS_DEVICE_DATA_ERROR;
            break;
        }
    }
    device->SpiFrames += (ULONG)count;
    WdfWaitLockRelease(device->StateLock);
    if (!NT_SUCCESS(status))
        goto Failed;
    if (!device->DiagnosticRecorded || !NT_SUCCESS(device->SpiLastStatus)) {
        Q2qSpiRecordStatus(device, STATUS_SUCCESS);
        device->DiagnosticRecorded = TRUE;
    }
    device->SpiLastStatus = STATUS_SUCCESS;
    Q2qSpiDeliver(device);
    return TRUE;
Failed:
    device->SpiLastStatus = status;
    WdfWaitLockAcquire(device->StateLock, NULL);
    Q2qStmCancelContacts(&device->TouchState, &device->InputReports);
    WdfWaitLockRelease(device->StateLock);
    Q2qSpiDeliver(device);
    Q2qSpiRecordStatus(device, status);
    return FALSE;
}
