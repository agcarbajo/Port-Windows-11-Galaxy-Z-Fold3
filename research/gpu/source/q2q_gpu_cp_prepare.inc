/* SPDX-License-Identifier: GPL-2.0-only
 * Owned typed CP buffers and source-derived GBIF unhalt, without CP start.
 */
static Q2Q_MMUI_BO *CpBos[3];
static PVOID CpCpus[3];
static ULONGLONG CpIovas[3];
static BOOLEAN CpBuffersAttempted, CpUnhaltAttempted;
NTSTATUS Q2qGpuBoCreateRing(Q2Q_MMUI_MEMORY *,Q2Q_MMUI_BO **);

static NTSTATUS gpu_cp_buffers_before_activation(VOID)
{
    static const SIZE_T sizes[3]={32768,4096,4096};
    SIZE_T bytes;
    ULONGLONG mair;
    ULONG i,j;
    NTSTATUS status;
    if (CpBuffersAttempted || !GpuBoOwner || !gpu_bo_gate()) return STATUS_INVALID_DEVICE_STATE;
    status=trace_val("cp_buffers_attempted",STATUS_SUCCESS,3);
    if (!NT_SUCCESS(status)) return status;
    CpBuffersAttempted=TRUE;
    for (i=0;i<3;++i) {
        if (!gpu_bo_gate()) return STATUS_DEVICE_NOT_READY;
        status=i==0?Q2qGpuBoCreateRing(GpuBoOwner,&CpBos[i]):
            Q2qMmuiBoCreate(GpuBoOwner,sizes[i],&CpBos[i]);
        if (!NT_SUCCESS(status)) return status;
        bytes=0;
        status=Q2qMmuiBoView(GpuBoOwner,CpBos[i],&CpCpus[i],&CpIovas[i],&bytes);
        if (!NT_SUCCESS(status)) return status;
        if (!CpCpus[i] || bytes!=sizes[i] || (CpIovas[i]&4095U) ||
                CpIovas[i]<0x100000000ULL || CpIovas[i]>0x100200000ULL-bytes)
            return STATUS_DEVICE_PROTOCOL_ERROR;
        for (j=0;j<i;++j)
            if (CpCpus[i]==CpCpus[j] || (CpIovas[i]<CpIovas[j]+sizes[j] &&
                    CpIovas[j]<CpIovas[i]+sizes[i])) return STATUS_DEVICE_PROTOCOL_ERROR;
        if (CpIovas[i]<GpuSqeIova+Q2Q_SQE_BO_BYTES &&
                GpuSqeIova<CpIovas[i]+bytes) return STATUS_DEVICE_PROTOCOL_ERROR;
        status=Q2qGpuBoMemoryContract(GpuBoOwner,CpBos[i],&mair);
        if (!NT_SUCCESS(status)) return status;
        RtlFillMemory(CpCpus[i],bytes,i==0?0xaa:i==1?0xa5:0);
        _DataSynchronizationBarrier();
        status=trace_val(i==0?"cp_ring_ro_nx_iova":i==1?"cp_result_rw_nx_iova":"cp_reglist_rw_nx_iova",
            STATUS_SUCCESS,CpIovas[i]);
        if (!NT_SUCCESS(status)) return status;
    }
    return trace_val("cp_buffers_staged_cpu_only",STATUS_SUCCESS,3);
}

static NTSTATUS gpu_cp_buffers_after_activation(VOID)
{
    static const ULONG sizes[3]={32768,4096,4096};
    ULONG i,j;
    ULONGLONG mair;
    NTSTATUS status;
    if (!CpBuffersAttempted || !gpu_gx_live_gate()) return STATUS_DEVICE_NOT_READY;
    for (i=0;i<3;++i) {
        if (!CpBos[i] || !CpCpus[i]) return STATUS_INVALID_DEVICE_STATE;
        status=Q2qGpuBoMemoryContract(GpuBoOwner,CpBos[i],&mair);
        if (!NT_SUCCESS(status)) return status;
        _DataSynchronizationBarrier();
        for (j=0;j<sizes[i];++j)
            if (((volatile UCHAR *)CpCpus[i])[j]!=(i==0?0xaa:i==1?0xa5:0))
                return STATUS_DEVICE_PROTOCOL_ERROR;
        if (!gpu_gx_live_gate()) return STATUS_DEVICE_NOT_READY;
    }
    return trace_val("cp_buffers_retained_cpu_verified",STATUS_SUCCESS,3);
}

static NTSTATUS gpu_cp_prepare(VOID)
{
    ULONGLONG start,now,elapsed=0;
    ULONG gx=0,gbif=0,gx_halt=0,gbif_halt=0,i=0;
    NTSTATUS status,logged;
    if (CpUnhaltAttempted || !gpu_gx_live_gate()) return STATUS_INVALID_DEVICE_STATE;
    status=gpu_cp_buffers_after_activation();
    if (!NT_SUCCESS(status)) return status;
    if (READ_REGISTER_ULONG((volatile ULONG *)(RunGfx+0xe0))!=0)
        return STATUS_DEVICE_PROTOCOL_ERROR;
    status=trace_val("cp_gbif_unhalt_attempted",STATUS_SUCCESS,2);
    if (!NT_SUCCESS(status)) return status;
    CpUnhaltAttempted=TRUE;
    /* Match q2q a6xx_deassert_gbif_halt: global first, then GX.
     * No reset, IRQ enable, SQE control, ring base or WPTR write. */
    if (!gpu_gx_live_gate()) return STATUS_DEVICE_NOT_READY;
    WRITE_REGISTER_ULONG((volatile ULONG *)(RunGfx+0xf114),0);
    _DataSynchronizationBarrier();
    if (!gpu_gx_live_gate()) return STATUS_DEVICE_NOT_READY;
    WRITE_REGISTER_ULONG((volatile ULONG *)(RunGfx+0x58),0);
    _DataSynchronizationBarrier();
    start=now_us(NULL);
    status=STATUS_IO_TIMEOUT;
    for (i=0;i<101;++i) {
        if (!gpu_gx_live_gate()) { status=STATUS_DEVICE_NOT_READY;break; }
        now=now_us(NULL);
        if (now<start) { status=STATUS_DEVICE_PROTOCOL_ERROR;break; }
        elapsed=now-start;
        if (elapsed>=100000U) break;
        gbif_halt=READ_REGISTER_ULONG((volatile ULONG *)(RunGfx+0xf114));
        gx_halt=READ_REGISTER_ULONG((volatile ULONG *)(RunGfx+0x58));
        gbif=READ_REGISTER_ULONG((volatile ULONG *)(RunGfx+0xf118));
        gx=READ_REGISTER_ULONG((volatile ULONG *)(RunGfx+0x5c));
        if (!gpu_gx_live_gate()) { status=STATUS_DEVICE_NOT_READY;break; }
        now=now_us(NULL);
        if (now<start) { status=STATUS_DEVICE_PROTOCOL_ERROR;break; }
        elapsed=now-start;
        if (elapsed>=100000U) break;
        if (!gbif_halt && !gx_halt && !(gbif&3U) && !(gx&1U)) {
            status=STATUS_SUCCESS;break;
        }
        if (delay_one_ms(NULL)!=0) { status=STATUS_DEVICE_NOT_READY;break; }
    }
    /* No synchronous file logging inside the bounded observation interval. */
    logged=trace_val("cp_gbif_unhalt_gbif_raw",status,gbif);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_gbif_unhalt_gx_raw",status,gx);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_gbif_unhalt_global_request",status,gbif_halt);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_gbif_unhalt_gx_request",status,gx_halt);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_gbif_unhalt_elapsed_us",status,elapsed);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_gbif_unhalt_sample_index",status,i);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_gbif_unhalt_result",status,2);
    return NT_SUCCESS(status)?logged:status;
}
