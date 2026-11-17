/*
 * q2qbatt.c - own KMDF battery miniclass for the Samsung SM-F926B (q2q), 2026-11-17.
 *
 * One Windows battery backed by the MAX77705 fuel gauge (I2C 0x36), which models the combined main+sub pack (no
 * per-pack SOC exists in hardware or in the vendor code, so two Windows batteries would need invented data).
 * Charger status from MAX77705 0x69; per-pack voltage/current from the two S2ASL01 limiters (0x38 main, 0x39 sub)
 * published read-only in a VOLATILE registry key  Enum\<instance>\Device Parameters\Q2qLive  (one value per field).
 * STRICTLY READ-ONLY on the buses: the only SPB transfers are "write 1-byte register address, read N bytes" for the
 * whitelisted registers; the charger and limiter configuration are never written (charging stays as the firmware /
 * Linux left it). Register formulas come from the Samsung q2q kernel sources (max77705 / s2asl01), checked on the device
 * with a read-only probe before this driver was written.
 *
 * _CRS connection order (SSDT Q2QBATD): [0] FG 0x36 (I2C8), [1] CHG 0x69 (I2C8), [2] LIM main 0x38 (I2C2),
 * [3] LIM sub 0x39 (IC10). Missing limiter connections only drop the diagnostics; the FG is mandatory.
 */
#include <ntddk.h>
#include <wdf.h>
#include <batclass.h>
#include <spb.h>
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include <ntstrsafe.h>

#define Q2Q_TAG 'tBqQ'
#define Q2Q_POLL_MS 15000
enum { CONN_FG = 0, CONN_CHG, CONN_LIM_MAIN, CONN_LIM_SUB, CONN_COUNT };

typedef struct _Q2Q_PACK { ULONG VbatMv; ULONG IchgMa; ULONG IdischgMa; BOOLEAN Valid; } Q2Q_PACK;

typedef struct _Q2Q_SAMPLE {
    LONG  Soc0p1;        /* RepSOC 0x06, 0.1 % */
    ULONG VcellMv;       /* 0x09 */
    LONG  CurrentMa;     /* 0x0A, + = into battery */
    LONG  AvgCurrentMa;  /* 0x0B */
    ULONG FullCapRepMah; /* 0x35 */
    ULONG DesignCapMah;  /* 0x18 */
    ULONG Cycles;        /* 0x17 */
    UCHAR ChgIntOk;      /* 0x69:0xB2 */
    UCHAR ChgDtls01;     /* 0x69:0xB4 */
    UCHAR ChgCnfg00;     /* 0x69:0xB7 */
    BOOLEAN ChgValid;
    Q2Q_PACK Pack[2];    /* 0 = main (0x38), 1 = sub (0x39) */
    BOOLEAN Valid;
} Q2Q_SAMPLE;

typedef struct _Q2Q_CTX {
    WDFDEVICE Device;
    WDFIOTARGET Target[CONN_COUNT];
    LARGE_INTEGER ConnId[CONN_COUNT];
    ULONG ConnCount;
    PVOID ClassHandle;
    ULONG Tag;
    ULONG NextTag;
    WDFWAITLOCK Lock;       /* bus + sample */
    WDFWAITLOCK ClassLock;  /* ClassHandle */
    WDFTIMER Timer;
    Q2Q_SAMPLE Last;
    ULONG LastPowerState;
    ULONG LastPercent;
    ULONG NominalMv;
    ULONG Failures;
    BATTERY_NOTIFY Notify;
    BOOLEAN NotifySet;
} Q2Q_CTX, *PQ2Q_CTX;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(Q2Q_CTX, Q2qGetCtx)

DRIVER_INITIALIZE DriverEntry;
static EVT_WDF_DRIVER_DEVICE_ADD Q2qDeviceAdd;
static EVT_WDF_DEVICE_PREPARE_HARDWARE Q2qPrepareHardware;
static EVT_WDF_DEVICE_RELEASE_HARDWARE Q2qReleaseHardware;
static EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT Q2qSelfManagedIoInit;
static EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP Q2qSelfManagedIoCleanup;
static EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL Q2qIoDeviceControl;
static EVT_WDF_TIMER Q2qTimer;

/* Read-only whitelist (register addresses). Anything else is refused before reaching the bus. */
static const UCHAR g_FgRegs[]  = { 0x00,0x05,0x06,0x09,0x0A,0x0B,0x0E,0x10,0x16,0x17,0x18,0x19,0x23,0x35,0xB2,0xFF };
static const UCHAR g_ChgRegs[] = { 0xB2,0xB3,0xB4,0xB7 };
static const UCHAR g_LimRegs[] = { 0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x1B,0x29 };

static BOOLEAN Q2qAllowed(ULONG conn, UCHAR reg)
{
    const UCHAR *list; ULONG n, i;
    if (conn == CONN_FG) { list = g_FgRegs; n = sizeof(g_FgRegs); }
    else if (conn == CONN_CHG) { list = g_ChgRegs; n = sizeof(g_ChgRegs); }
    else { list = g_LimRegs; n = sizeof(g_LimRegs); }
    for (i = 0; i < n; i++) if (list[i] == reg) return TRUE;
    return FALSE;
}

/* SPB sequence: [write 1 byte = register address][read len bytes]. No other write is ever issued. */
static NTSTATUS Q2qReadRegs(PQ2Q_CTX c, ULONG conn, UCHAR reg, PUCHAR out, ULONG len)
{
    SPB_TRANSFER_LIST_AND_ENTRIES(2) seq;
    WDF_MEMORY_DESCRIPTOR in;
    WDF_REQUEST_SEND_OPTIONS opt;
    ULONG_PTR done = 0;
    UCHAR address = reg;
    NTSTATUS st;
    if (conn >= c->ConnCount || !c->Target[conn] || len == 0 || len > 2 || !Q2qAllowed(conn, reg))
        return STATUS_ACCESS_DENIED;
    SPB_TRANSFER_LIST_INIT(&(seq.List), 2);
    seq.List.Transfers[0] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionToDevice, 0, &address, 1);
    seq.List.Transfers[1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionFromDevice, 0, out, len);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&in, &seq, sizeof(seq));
    WDF_REQUEST_SEND_OPTIONS_INIT(&opt, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&opt, WDF_REL_TIMEOUT_IN_MS(500));
    st = WdfIoTargetSendIoctlSynchronously(c->Target[conn], NULL, IOCTL_SPB_EXECUTE_SEQUENCE, &in, NULL, &opt, &done);
    if (NT_SUCCESS(st) && done != 1 + len) st = STATUS_DEVICE_PROTOCOL_ERROR;
    return st;
}

static NTSTATUS Q2qRead16(PQ2Q_CTX c, UCHAR reg, PUSHORT v)
{
    UCHAR b[2]; NTSTATUS st = Q2qReadRegs(c, CONN_FG, reg, b, 2);
    if (NT_SUCCESS(st)) *v = (USHORT)((b[1] << 8) | b[0]);   /* little endian, as max77705_bulk_read users */
    return st;
}

/* fg-registers.json: VCELL ~ 78.125 uV/LSB (exact integer form of max77705_fg_read_vcell). */
static ULONG Q2qVcellMv(USHORT w)
{
    ULONG v = ((ULONG)(w & 0xFFF) * 78125) / 1000000;
    v += (((ULONG)((w & 0xF000) >> 4) * 78125) / 1000000) << 4;
    return v;
}
/* CURRENT/AVGCURRENT: two's complement, 15625*fg_resistor/100000 mA per LSB, fg_resistor = 2 (DT). */
static LONG Q2qCurrentMa(USHORT w)
{
    ULONG t = w; LONG ma; BOOLEAN neg = (w & 0x8000) != 0;
    if (neg) t = ((~t) & 0xFFFF) + 1;
    ma = (LONG)((t * 15625 * 2) / 100000);
    return neg ? -ma : ma;
}
/* SOCREP: 0.1 % = ((hi*100) + (lo*100/256)) / 10, clamp 1000. */
static LONG Q2qSoc0p1(USHORT w)
{
    LONG s = ((((w >> 8) & 0xFF) * 100) + (((w & 0xFF) * 100) / 256)) / 10;
    return s > 1000 ? 1000 : s;
}
/* Capacity registers: mAh = raw * fg_resistor / 2 (= raw with fg_resistor 2). */
static ULONG Q2qCapMah(USHORT w) { return ((ULONG)w * 2) / 2; }

static VOID Q2qSamplePack(PQ2Q_CTX c, ULONG conn, Q2Q_PACK *p)
{
    UCHAR r[6];
    ULONG i;
    RtlZeroMemory(p, sizeof(*p));
    for (i = 0; i < 6; i++)
        if (!NT_SUCCESS(Q2qReadRegs(c, conn, (UCHAR)(0x09 + i), &r[i], 1))) return;
    /* s2asl01: raw12 = (VAL1 << 4) | (VAL2 >> 4); VBAT 1.25 mV/LSB, ICHG/IDISCHG 1.5625 mA/LSB */
    p->VbatMv = (((ULONG)r[0] << 4) | (r[1] >> 4)) * 1250 / 1000;
    p->IchgMa = (((ULONG)r[2] << 4) | (r[3] >> 4)) * 15625 / 10 / 1000;
    p->IdischgMa = (((ULONG)r[4] << 4) | (r[5] >> 4)) * 15625 / 10 / 1000;
    p->Valid = TRUE;
}

static NTSTATUS Q2qSample(PQ2Q_CTX c, Q2Q_SAMPLE *s)
{
    USHORT w; NTSTATUS st; UCHAR b;
    RtlZeroMemory(s, sizeof(*s));
    if (!NT_SUCCESS(st = Q2qRead16(c, 0x06, &w))) return st; s->Soc0p1 = Q2qSoc0p1(w);
    if (!NT_SUCCESS(st = Q2qRead16(c, 0x09, &w))) return st; s->VcellMv = Q2qVcellMv(w);
    if (!NT_SUCCESS(st = Q2qRead16(c, 0x0A, &w))) return st; s->CurrentMa = Q2qCurrentMa(w);
    if (!NT_SUCCESS(st = Q2qRead16(c, 0x0B, &w))) return st; s->AvgCurrentMa = Q2qCurrentMa(w);
    if (!NT_SUCCESS(st = Q2qRead16(c, 0x35, &w))) return st; s->FullCapRepMah = Q2qCapMah(w);
    if (!NT_SUCCESS(st = Q2qRead16(c, 0x18, &w))) return st; s->DesignCapMah = Q2qCapMah(w);
    if (!NT_SUCCESS(st = Q2qRead16(c, 0x17, &w))) return st; s->Cycles = w;
    if (NT_SUCCESS(Q2qReadRegs(c, CONN_CHG, 0xB2, &b, 1))) {
        s->ChgIntOk = b;
        s->ChgValid = TRUE;
        if (NT_SUCCESS(Q2qReadRegs(c, CONN_CHG, 0xB4, &b, 1))) s->ChgDtls01 = b; else s->ChgValid = FALSE;
        if (NT_SUCCESS(Q2qReadRegs(c, CONN_CHG, 0xB7, &b, 1))) s->ChgCnfg00 = b; else s->ChgValid = FALSE;
    }
    Q2qSamplePack(c, CONN_LIM_MAIN, &s->Pack[0]);
    Q2qSamplePack(c, CONN_LIM_SUB, &s->Pack[1]);
    if (s->FullCapRepMah == 0 || s->DesignCapMah == 0) return STATUS_DEVICE_DATA_ERROR;
    s->Valid = TRUE;
    return STATUS_SUCCESS;
}

static ULONG Q2qFullMwh(PQ2Q_CTX c, const Q2Q_SAMPLE *s) { return s->FullCapRepMah * c->NominalMv / 1000; }

static ULONG Q2qPowerState(const Q2Q_SAMPLE *s)
{
    ULONG ps = 0;
    if (s->ChgValid && (s->ChgIntOk & 0x60)) ps |= BATTERY_POWER_ON_LINE;      /* CHGIN_OK | WCIN_OK */
    if ((ps & BATTERY_POWER_ON_LINE) && s->AvgCurrentMa > 0) ps |= BATTERY_CHARGING;
    if (s->AvgCurrentMa < 0) ps |= BATTERY_DISCHARGING;
    if (s->Soc0p1 < 40 && (ps & BATTERY_DISCHARGING)) ps |= BATTERY_CRITICAL;
    return ps;
}

static VOID Q2qPublish(PQ2Q_CTX c, const Q2Q_SAMPLE *s)
{
    WDFKEY parent = NULL, live = NULL;
    DECLARE_CONST_UNICODE_STRING(name, L"Q2qLive");
    struct { PCWSTR n; ULONG v; } vals[] = {
        { L"FgSoc_0p1pct", (ULONG)s->Soc0p1 }, { L"FgVcell_mV", s->VcellMv }, { L"FgCurrent_mA", (ULONG)s->CurrentMa },
        { L"FgAvgCurrent_mA", (ULONG)s->AvgCurrentMa }, { L"FgFullCapRep_mAh", s->FullCapRepMah },
        { L"FgDesignCap_mAh", s->DesignCapMah }, { L"FgCycles", s->Cycles }, { L"ChgIntOk", s->ChgIntOk },
        { L"ChgDetails01", s->ChgDtls01 }, { L"ChgCnfg00", s->ChgCnfg00 }, { L"ChgValid", s->ChgValid },
        { L"MainValid", s->Pack[0].Valid }, { L"MainVbat_mV", s->Pack[0].VbatMv }, { L"MainIchg_mA", s->Pack[0].IchgMa },
        { L"MainIdischg_mA", s->Pack[0].IdischgMa }, { L"SubValid", s->Pack[1].Valid },
        { L"SubVbat_mV", s->Pack[1].VbatMv }, { L"SubIchg_mA", s->Pack[1].IchgMa },
        { L"SubIdischg_mA", s->Pack[1].IdischgMa }, { L"PowerState", Q2qPowerState(s) }, { L"Failures", c->Failures },
    };
    ULONG i;
    if (!NT_SUCCESS(WdfDeviceOpenRegistryKey(c->Device, PLUGPLAY_REGKEY_DEVICE, KEY_CREATE_SUB_KEY,
                                              WDF_NO_OBJECT_ATTRIBUTES, &parent))) return;
    if (NT_SUCCESS(WdfRegistryCreateKey(parent, &name, KEY_SET_VALUE, REG_OPTION_VOLATILE, NULL,
                                        WDF_NO_OBJECT_ATTRIBUTES, &live))) {
        for (i = 0; i < ARRAYSIZE(vals); i++) {
            UNICODE_STRING vn;
            RtlInitUnicodeString(&vn, vals[i].n);
            WdfRegistryAssignULong(live, &vn, vals[i].v);
        }
        WdfRegistryClose(live);
    }
    WdfRegistryClose(parent);
}

/* Poll: refresh the sample, publish it, notify the battery class on a state / percent change or a threshold. */
static VOID Q2qRefresh(PQ2Q_CTX c, BOOLEAN notify)
{
    Q2Q_SAMPLE s;
    NTSTATUS st;
    ULONG ps, pct, cap;
    BOOLEAN changed = FALSE;
    WdfWaitLockAcquire(c->Lock, NULL);
    st = Q2qSample(c, &s);
    if (NT_SUCCESS(st)) {
        c->Failures = 0;
        if (!c->Last.Valid) changed = TRUE;
        c->Last = s;
    } else if (++c->Failures >= 4 && c->Last.Valid) {
        RtlZeroMemory(&c->Last, sizeof(c->Last));  /* 60 s without a good sample: report not present */
        c->Tag = BATTERY_TAG_INVALID;
        changed = TRUE;
    }
    if (c->Last.Valid) {
        ps = Q2qPowerState(&c->Last);
        pct = (ULONG)c->Last.Soc0p1 / 10;
        cap = (ULONG)c->Last.Soc0p1 * Q2qFullMwh(c, &c->Last) / 1000;
        if (ps != c->LastPowerState || pct != c->LastPercent) changed = TRUE;
        if (c->NotifySet && ((c->Notify.PowerState && !(c->Notify.PowerState & ps)) ||
            cap < c->Notify.LowCapacity || cap > c->Notify.HighCapacity)) changed = TRUE;
        c->LastPowerState = ps;
        c->LastPercent = pct;
        if (c->Tag == BATTERY_TAG_INVALID) c->Tag = ++c->NextTag;
        Q2qPublish(c, &c->Last);
    }
    WdfWaitLockRelease(c->Lock);
    if (notify && changed) {
        WdfWaitLockAcquire(c->ClassLock, NULL);
        if (c->ClassHandle) BatteryClassStatusNotify(c->ClassHandle);
        WdfWaitLockRelease(c->ClassLock);
    }
}

/* ---- battery class callbacks ---- */
static NTSTATUS Q2qQueryTag(PVOID Context, PULONG BatteryTag)
{
    PQ2Q_CTX c = (PQ2Q_CTX)Context;
    WdfWaitLockAcquire(c->Lock, NULL);
    *BatteryTag = c->Last.Valid ? c->Tag : BATTERY_TAG_INVALID;
    WdfWaitLockRelease(c->Lock);
    return *BatteryTag == BATTERY_TAG_INVALID ? STATUS_NO_SUCH_DEVICE : STATUS_SUCCESS;
}

static NTSTATUS Q2qQueryInformation(PVOID Context, ULONG BatteryTag, BATTERY_QUERY_INFORMATION_LEVEL Level,
                                    LONG AtRate, PVOID Buffer, ULONG BufferLength, PULONG ReturnedLength)
{
    PQ2Q_CTX c = (PQ2Q_CTX)Context;
    BATTERY_INFORMATION bi;
    ULONG ul, n = 0;
    PVOID src = NULL;
    Q2Q_SAMPLE s;
    static const WCHAR device[] = L"q2q dual pack (MAX77705)";
    static const WCHAR maker[] = L"Samsung";
    static const WCHAR unique[] = L"q2q-max77705-fg";
    UNREFERENCED_PARAMETER(AtRate);
    WdfWaitLockAcquire(c->Lock, NULL);
    s = c->Last;
    WdfWaitLockRelease(c->Lock);
    if (!s.Valid || BatteryTag != c->Tag) return STATUS_NO_SUCH_DEVICE;
    switch (Level) {
    case BatteryInformation:
        RtlZeroMemory(&bi, sizeof(bi));
        bi.Capabilities = BATTERY_SYSTEM_BATTERY;   /* no SET_* capabilities: Windows cannot control charging */
        bi.Technology = 1;
        RtlCopyMemory(bi.Chemistry, "LION", 4);
        bi.DesignedCapacity = s.DesignCapMah * c->NominalMv / 1000;
        bi.FullChargedCapacity = Q2qFullMwh(c, &s);
        bi.DefaultAlert1 = bi.FullChargedCapacity * 7 / 100;
        bi.DefaultAlert2 = bi.FullChargedCapacity * 4 / 100;
        bi.CriticalBias = 0;
        bi.CycleCount = s.Cycles;
        src = &bi; n = sizeof(bi);
        break;
    case BatteryDeviceName: src = (PVOID)device; n = sizeof(device); break;
    case BatteryManufactureName: src = (PVOID)maker; n = sizeof(maker); break;
    case BatteryUniqueID: src = (PVOID)unique; n = sizeof(unique); break;
    case BatteryEstimatedTime:
        ul = BATTERY_UNKNOWN_TIME;
        if (s.AvgCurrentMa < -10)  /* seconds = remaining mAh / discharge mA * 3600 */
            ul = (ULONG)(((ULONGLONG)s.Soc0p1 * s.FullCapRepMah / 1000) * 3600 / (ULONG)(-s.AvgCurrentMa));
        src = &ul; n = sizeof(ul);
        break;
    case BatteryTemperature:         /* FG TEMPERATURE is stale (thermal_source = ADC) */
    case BatteryGranularityInformation:
    case BatteryManufactureDate:
    case BatterySerialNumber:
    default:
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    if (BufferLength < n) return STATUS_BUFFER_TOO_SMALL;
    RtlCopyMemory(Buffer, src, n);
    *ReturnedLength = n;
    return STATUS_SUCCESS;
}

static NTSTATUS Q2qQueryStatus(PVOID Context, ULONG BatteryTag, PBATTERY_STATUS Status)
{
    PQ2Q_CTX c = (PQ2Q_CTX)Context;
    Q2Q_SAMPLE s;
    WdfWaitLockAcquire(c->Lock, NULL);
    if (NT_SUCCESS(Q2qSample(c, &s))) { c->Last = s; c->Failures = 0; }
    s = c->Last;
    WdfWaitLockRelease(c->Lock);
    if (!s.Valid || BatteryTag != c->Tag) return STATUS_NO_SUCH_DEVICE;
    RtlZeroMemory(Status, sizeof(*Status));
    Status->PowerState = Q2qPowerState(&s);
    Status->Capacity = (ULONG)s.Soc0p1 * Q2qFullMwh(c, &s) / 1000;
    Status->Voltage = s.VcellMv;
    Status->Rate = (LONG)((LONGLONG)s.AvgCurrentMa * (LONGLONG)s.VcellMv / 1000);  /* mW, - = discharging */
    return STATUS_SUCCESS;
}

static NTSTATUS Q2qSetStatusNotify(PVOID Context, ULONG BatteryTag, PBATTERY_NOTIFY Notify)
{
    PQ2Q_CTX c = (PQ2Q_CTX)Context;
    if (BatteryTag != c->Tag) return STATUS_NO_SUCH_DEVICE;
    WdfWaitLockAcquire(c->Lock, NULL);
    c->Notify = *Notify;
    c->NotifySet = TRUE;
    WdfWaitLockRelease(c->Lock);
    return STATUS_SUCCESS;
}

static NTSTATUS Q2qDisableStatusNotify(PVOID Context)
{
    PQ2Q_CTX c = (PQ2Q_CTX)Context;
    WdfWaitLockAcquire(c->Lock, NULL);
    c->NotifySet = FALSE;
    WdfWaitLockRelease(c->Lock);
    return STATUS_SUCCESS;
}

static NTSTATUS Q2qSetInformation(PVOID Context, ULONG BatteryTag, BATTERY_SET_INFORMATION_LEVEL Level, PVOID Buffer)
{
    UNREFERENCED_PARAMETER(Context); UNREFERENCED_PARAMETER(BatteryTag); UNREFERENCED_PARAMETER(Level);
    UNREFERENCED_PARAMETER(Buffer);
    return STATUS_NOT_SUPPORTED;  /* never change charger / limiter state */
}

/* ---- WDF plumbing ---- */
static VOID Q2qTimer(WDFTIMER Timer)
{
    PQ2Q_CTX c = Q2qGetCtx(WdfTimerGetParentObject(Timer));
    Q2qRefresh(c, TRUE);
    WdfTimerStart(Timer, WDF_REL_TIMEOUT_IN_MS(Q2Q_POLL_MS));  /* passive-level timers cannot be periodic */
}

static NTSTATUS Q2qPrepareHardware(WDFDEVICE Device, WDFCMRESLIST Raw, WDFCMRESLIST Translated)
{
    PQ2Q_CTX c = Q2qGetCtx(Device);
    ULONG i, count = WdfCmResourceListGetCount(Translated);
    NTSTATUS st;
    UNREFERENCED_PARAMETER(Raw);
    c->ConnCount = 0;
    for (i = 0; i < count && c->ConnCount < CONN_COUNT; i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d = WdfCmResourceListGetDescriptor(Translated, i);
        if (d && d->Type == CmResourceTypeConnection && d->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_SERIAL &&
            d->u.Connection.Type == CM_RESOURCE_CONNECTION_TYPE_SERIAL_I2C) {
            c->ConnId[c->ConnCount].LowPart = d->u.Connection.IdLowPart;
            c->ConnId[c->ConnCount].HighPart = d->u.Connection.IdHighPart;
            c->ConnCount++;
        }
    }
    if (c->ConnCount == 0) return STATUS_INSUFFICIENT_RESOURCES;
    for (i = 0; i < c->ConnCount; i++) {
        WDF_IO_TARGET_OPEN_PARAMS open;
        DECLARE_UNICODE_STRING_SIZE(path, RESOURCE_HUB_PATH_SIZE);
        st = RESOURCE_HUB_CREATE_PATH_FROM_ID(&path, c->ConnId[i].LowPart, c->ConnId[i].HighPart);
        if (!NT_SUCCESS(st)) return st;
        st = WdfIoTargetCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &c->Target[i]);
        if (!NT_SUCCESS(st)) return st;
        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open, &path, GENERIC_READ | GENERIC_WRITE);
        st = WdfIoTargetOpen(c->Target[i], &open);
        if (!NT_SUCCESS(st)) {
            WdfObjectDelete(c->Target[i]);
            c->Target[i] = NULL;
            if (i == CONN_FG) return st;  /* the gauge is mandatory; the others only feed diagnostics */
        }
    }
    return STATUS_SUCCESS;
}

static NTSTATUS Q2qReleaseHardware(WDFDEVICE Device, WDFCMRESLIST Translated)
{
    PQ2Q_CTX c = Q2qGetCtx(Device);
    ULONG i;
    UNREFERENCED_PARAMETER(Translated);
    for (i = 0; i < CONN_COUNT; i++) {
        if (c->Target[i]) { WdfIoTargetClose(c->Target[i]); WdfObjectDelete(c->Target[i]); c->Target[i] = NULL; }
    }
    c->ConnCount = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS Q2qSelfManagedIoInit(WDFDEVICE Device)
{
    PQ2Q_CTX c = Q2qGetCtx(Device);
    BATTERY_MINIPORT_INFO_V1_1 info;
    NTSTATUS st;
    Q2qRefresh(c, FALSE);
    RtlZeroMemory(&info, sizeof(info));
    info.MajorVersion = BATTERY_CLASS_MAJOR_VERSION;
    info.MinorVersion = BATTERY_CLASS_MINOR_VERSION_1;
    info.Context = c;
    info.QueryTag = Q2qQueryTag;
    info.QueryInformation = Q2qQueryInformation;
    info.SetInformation = Q2qSetInformation;
    info.QueryStatus = Q2qQueryStatus;
    info.SetStatusNotify = Q2qSetStatusNotify;
    info.DisableStatusNotify = Q2qDisableStatusNotify;
    info.Pdo = WdfDeviceWdmGetPhysicalDevice(Device);
    info.DeviceName = NULL;
    info.Fdo = WdfDeviceWdmGetDeviceObject(Device);
    WdfWaitLockAcquire(c->ClassLock, NULL);
    st = BatteryClassInitializeDevice((PBATTERY_MINIPORT_INFO)&info, &c->ClassHandle);
    WdfWaitLockRelease(c->ClassLock);
    if (!NT_SUCCESS(st)) return st;
    WdfTimerStart(c->Timer, WDF_REL_TIMEOUT_IN_MS(Q2Q_POLL_MS));
    return STATUS_SUCCESS;
}

static VOID Q2qSelfManagedIoCleanup(WDFDEVICE Device)
{
    PQ2Q_CTX c = Q2qGetCtx(Device);
    WdfTimerStop(c->Timer, TRUE);
    WdfWaitLockAcquire(c->ClassLock, NULL);
    if (c->ClassHandle) { BatteryClassUnload(c->ClassHandle); c->ClassHandle = NULL; }
    WdfWaitLockRelease(c->ClassLock);
}

static VOID Q2qIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutLen, size_t InLen, ULONG Code)
{
    PQ2Q_CTX c = Q2qGetCtx(WdfIoQueueGetDevice(Queue));
    NTSTATUS st = STATUS_NOT_SUPPORTED;
    UNREFERENCED_PARAMETER(OutLen); UNREFERENCED_PARAMETER(InLen); UNREFERENCED_PARAMETER(Code);
    WdfWaitLockAcquire(c->ClassLock, NULL);
    if (c->ClassHandle) st = BatteryClassIoctl(c->ClassHandle, WdfRequestWdmGetIrp(Request));
    WdfWaitLockRelease(c->ClassLock);
    if (st == STATUS_NOT_SUPPORTED) WdfRequestComplete(Request, st);  /* otherwise battc owns the IRP */
}

static NTSTATUS Q2qDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT Init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_OBJECT_ATTRIBUTES attr;
    WDF_IO_QUEUE_CONFIG qc;
    WDF_TIMER_CONFIG tc;
    WDFDEVICE device;
    PQ2Q_CTX c;
    NTSTATUS st;
    WDFKEY key;
    UNREFERENCED_PARAMETER(Driver);
    WdfDeviceInitSetPowerPolicyOwnership(Init, TRUE);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = Q2qPrepareHardware;
    pnp.EvtDeviceReleaseHardware = Q2qReleaseHardware;
    pnp.EvtDeviceSelfManagedIoInit = Q2qSelfManagedIoInit;
    pnp.EvtDeviceSelfManagedIoCleanup = Q2qSelfManagedIoCleanup;
    WdfDeviceInitSetPnpPowerEventCallbacks(Init, &pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, Q2Q_CTX);
    st = WdfDeviceCreate(&Init, &attr, &device);
    if (!NT_SUCCESS(st)) return st;
    c = Q2qGetCtx(device);
    RtlZeroMemory(c, sizeof(*c));
    c->Device = device;
    c->Tag = BATTERY_TAG_INVALID;
    c->NominalMv = 3880;
    if (NT_SUCCESS(WdfDeviceOpenRegistryKey(device, PLUGPLAY_REGKEY_DEVICE, KEY_READ, WDF_NO_OBJECT_ATTRIBUTES, &key))) {
        DECLARE_CONST_UNICODE_STRING(nv, L"NominalVoltage_mV");
        ULONG v = 0;
        if (NT_SUCCESS(WdfRegistryQueryULong(key, &nv, &v)) && v >= 3000 && v <= 4500) c->NominalMv = v;
        WdfRegistryClose(key);
    }
    WDF_OBJECT_ATTRIBUTES_INIT(&attr);
    attr.ParentObject = device;
    if (!NT_SUCCESS(st = WdfWaitLockCreate(&attr, &c->Lock))) return st;
    if (!NT_SUCCESS(st = WdfWaitLockCreate(&attr, &c->ClassLock))) return st;
    WDF_TIMER_CONFIG_INIT(&tc, Q2qTimer);  /* one-shot, re-armed by Q2qTimer (passive timers cannot be periodic) */
    tc.AutomaticSerialization = FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&attr);
    attr.ParentObject = device;
    attr.ExecutionLevel = WdfExecutionLevelPassive;
    if (!NT_SUCCESS(st = WdfTimerCreate(&tc, &attr, &c->Timer))) return st;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qc, WdfIoQueueDispatchParallel);
    qc.EvtIoDeviceControl = Q2qIoDeviceControl;
    qc.PowerManaged = WdfFalse;
    if (!NT_SUCCESS(st = WdfIoQueueCreate(device, &qc, WDF_NO_OBJECT_ATTRIBUTES, NULL))) return st;
    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG cfg;
    WDF_DRIVER_CONFIG_INIT(&cfg, Q2qDeviceAdd);
    cfg.DriverPoolTag = Q2Q_TAG;
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &cfg, WDF_NO_HANDLE);
}
