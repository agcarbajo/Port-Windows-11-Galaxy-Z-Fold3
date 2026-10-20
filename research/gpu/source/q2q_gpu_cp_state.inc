/* SPDX-License-Identifier: GPL-2.0-only
 * Direct source-listed status reads AFTER the CP guard, no debug selectors.
 */
static BOOLEAN CpStateAttempted;

static NTSTATUS gpu_cp_state_snapshot(VOID)
{
    static const ULONG indices[]={0x210,0x213,0x812,0x823,0x824,0x825};
    static const char *names[]={"cp_after_rbbm_status_raw","cp_after_rbbm_status3_raw",
        "cp_after_cp2gmu_status_raw","cp_after_interrupt_status_raw",
        "cp_after_protect_status_raw","cp_after_status1_raw"};
    ULONG values[6]={0},count=0,i;
    NTSTATUS status=STATUS_SUCCESS,logged;
    if (CpStateAttempted || !CpExecAttempted || !gpu_gx_live_gate())
        return STATUS_INVALID_DEVICE_STATE;
    CpStateAttempted=TRUE;
    for (count=0;count<6;++count) {
        status=cp_read(indices[count],&values[count]);
        if (!NT_SUCCESS(status)) break;
    }
    logged=trace_val("cp_after_state_read_result",status,count);
    for (i=0;i<count && NT_SUCCESS(logged);++i)
        logged=trace_val(names[i],STATUS_SUCCESS,values[i]);
    return NT_SUCCESS(status)?logged:status;
}
