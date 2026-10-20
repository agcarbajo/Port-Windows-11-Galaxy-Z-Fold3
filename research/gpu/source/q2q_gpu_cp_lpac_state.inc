/* SPDX-License-Identifier: GPL-2.0-only
 * Nine direct post-guard reads; LPAC bases are observations, never programmed.
 */
static BOOLEAN CpStateAttempted;
static BOOLEAN LpacInitialAttempted;

static NTSTATUS gpu_lpac_initial_snapshot(VOID)
{
    static const ULONG indices[]={0xb81,0xb82,0xb83};
    static const char *names[]={"cp_initial_lpac_control_raw","cp_initial_lpac_base_low_raw",
        "cp_initial_lpac_base_high_raw"};
    ULONG value=0,i;
    NTSTATUS status,logged;
    if (LpacInitialAttempted || CpExecAttempted || !gpu_gx_live_gate())
        return STATUS_INVALID_DEVICE_STATE;
    LpacInitialAttempted=TRUE;
    for (i=0;i<3;++i) {
        status=cp_read(indices[i],&value);
        logged=trace_val(names[i],status,value);
        if (!NT_SUCCESS(status)) return status;
        if (!NT_SUCCESS(logged)) return logged;
        if (value) return STATUS_DEVICE_PROTOCOL_ERROR;
    }
    return trace_val("cp_initial_lpac_zero_reads_verified",STATUS_SUCCESS,3);
}

static NTSTATUS gpu_cp_state_snapshot(VOID)
{
    static const ULONG indices[]={0x210,0x213,0x812,0x823,0x824,0x825,0xb81,0xb82,0xb83};
    static const char *names[]={"cp_after_rbbm_status_raw","cp_after_rbbm_status3_raw",
        "cp_after_cp2gmu_status_raw","cp_after_interrupt_status_raw",
        "cp_after_protect_status_raw","cp_after_status1_raw",
        "cp_after_lpac_sqe_control_raw","cp_after_lpac_sqe_base_low_raw",
        "cp_after_lpac_sqe_base_high_raw"};
    ULONG values[9]={0},count=0,i;
    NTSTATUS status=STATUS_SUCCESS,logged;
    if (CpStateAttempted || !CpExecAttempted || !gpu_gx_live_gate())
        return STATUS_INVALID_DEVICE_STATE;
    CpStateAttempted=TRUE;
    for (count=0;count<9;++count) {
        status=cp_read(indices[count],&values[count]);
        if (!NT_SUCCESS(status)) break;
    }
    logged=trace_val("cp_after_state_read_result",status,count);
    for (i=0;i<count && NT_SUCCESS(logged);++i)
        logged=trace_val(names[i],STATUS_SUCCESS,values[i]);
    return NT_SUCCESS(status)?logged:status;
}
