/* SPDX-License-Identifier: GPL-2.0-only
 * Complete CP address pairs and configuration, without writes or idle claims.
 * Indices are audited against the local q2q 754466 source at build time.
 */
static BOOLEAN CpPreflightAttempted;

static NTSTATUS gpu_cp_preflight(VOID)
{
    static const ULONG indices[] = {
        0x38,0x830,0x831,0x800,0x801,0x802,0x804,0x805,
        0x808,0x842,0x844,0x8c1,0x8c2,0x8c3,0xb34,0x841,
        0x98d,0x16,0x17,0x3c45,0x3c46
    };
    static const char *names[] = {
        "cp_pre_irq_mask","cp_pre_sqe_base_lo","cp_pre_sqe_base_hi",
        "cp_pre_rb_base_lo","cp_pre_rb_base_hi","cp_pre_rb_control",
        "cp_pre_rptr_addr_lo","cp_pre_rptr_addr_hi","cp_pre_sqe_control",
        "cp_pre_address_mode","cp_pre_apriv","cp_pre_roq1","cp_pre_roq2",
        "cp_pre_pool_size","cp_pre_lpac_fifo","cp_pre_chicken",
        "cp_pre_ahb_control","cp_pre_gx_halt","cp_pre_gx_halt_ack",
        "cp_pre_gbif_halt","cp_pre_gbif_halt_ack"
    };
    ULONG i, value;
    NTSTATUS status;
    if (CpPreflightAttempted || !gpu_gx_live_gate()) return STATUS_INVALID_DEVICE_STATE;
    status=trace_val("cp_preflight_attempted",STATUS_SUCCESS,21);
    if (!NT_SUCCESS(status)) return status;
    CpPreflightAttempted=TRUE;
    for (i=0;i<21;++i) {
        if (!gpu_gx_live_gate()) return STATUS_DEVICE_NOT_READY;
        value=READ_REGISTER_ULONG((volatile ULONG *)(RunGfx+indices[i]*4));
        status=trace_val(names[i],STATUS_SUCCESS,value);
        if (!NT_SUCCESS(status)) return status;
        if (!gpu_gx_live_gate()) return STATUS_DEVICE_NOT_READY;
    }
    return trace_val("cp_preflight_complete",STATUS_SUCCESS,21);
}
