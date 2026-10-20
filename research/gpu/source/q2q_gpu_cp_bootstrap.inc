/* SPDX-License-Identifier: GPL-2.0-only
 * One owned CP stream, polling a private memory token; no secure transition.
 * Full rendering, optional LLCC/HWCG/preemption and IRQ delivery are not enabled.
 */
#include "q2q_cp_init_contract.h"
#define Q2Q_CP_TOKEN 0x660c0001U
#define Q2Q_CP_SENTINEL 0x660bad00U
static BOOLEAN CpExecAttempted;

static NTSTATUS cp_read(ULONG index,ULONG *value)
{
    if (!value || index>=0x10000 || !gpu_gx_live_gate()) return STATUS_DEVICE_NOT_READY;
    *value=READ_REGISTER_ULONG((volatile ULONG *)(RunGfx+index*4));
    return gpu_gx_live_gate()?STATUS_SUCCESS:STATUS_DEVICE_NOT_READY;
}

static NTSTATUS cp_write_checked(ULONG index,ULONG value)
{
    ULONG actual=0;
    NTSTATUS status;
    if (index>=0x10000 || !gpu_gx_live_gate()) return STATUS_DEVICE_NOT_READY;
    WRITE_REGISTER_ULONG((volatile ULONG *)(RunGfx+index*4),value);
    _DataSynchronizationBarrier();
    status=cp_read(index,&actual);
    if (!NT_SUCCESS(status)) return status;
    if (actual!=value) {
        trace_val("cp_exec_write_mismatch_index",STATUS_DEVICE_PROTOCOL_ERROR,index);
        trace_val("cp_exec_write_mismatch_actual",STATUS_DEVICE_PROTOCOL_ERROR,actual);
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }
    return STATUS_SUCCESS;
}

static ULONG cp_packet(ULONG opcode,ULONG count)
{
    ULONG x=opcode,y=count,po=1,pc=1;
    while (x) { po^=x&1U;x>>=1; }
    while (y) { pc^=y&1U;y>>=1; }
    return 0x70000000U|(po<<23)|(opcode<<16)|(pc<<15)|count;
}

static NTSTATUS gpu_cp_prepare(VOID)
{
    volatile ULONG *result;
    ULONG *ring,*list;
    ULONG value=0,i,token=0,rptr=0,wptr=0,shadow=0,fault=0,irq=0,fence=0;
    ULONGLONG start,now,elapsed=0;
    NTSTATUS status,logged;
    static const ULONG initial_zero[]={0x38,0x201,0x800,0x801,0x804,0x805,0x806,0x807,0x830,0x831,0x808,0x821,0xae};
    if (CpExecAttempted || !gpu_gx_live_gate()) return STATUS_INVALID_DEVICE_STATE;
    status=gpu_cp_buffers_after_activation();
    if (!NT_SUCCESS(status)) return status;
    status=gpu_sqe_content_check();
    if (!NT_SUCCESS(status)) return status;
    for (i=0;i<sizeof(initial_zero)/sizeof(initial_zero[0]);++i) {
        status=cp_read(initial_zero[i],&value);
        if (!NT_SUCCESS(status)) return status;
        if (value) {
            trace_val("cp_exec_initial_nonzero_index",STATUS_DEVICE_PROTOCOL_ERROR,initial_zero[i]);
            trace_val("cp_exec_initial_nonzero_value",STATUS_DEVICE_PROTOCOL_ERROR,value);
            return STATUS_DEVICE_PROTOCOL_ERROR;
        }
    }
    ring=CpCpus[0];result=CpCpus[1];list=CpCpus[2];
    result[0]=0;result[16]=Q2Q_CP_SENTINEL;
    _DataSynchronizationBarrier();
    if (result[0]!=0 || result[16]!=Q2Q_CP_SENTINEL) return STATUS_DEVICE_PROTOCOL_ERROR;
    status=trace_val("cp_exec_attempted",STATUS_SUCCESS,1);
    if (!NT_SUCCESS(status)) return status;
    CpExecAttempted=TRUE;
    /* Original BSP stop encoding, not an inference that initial control0 halted CP.
     * Bases/pointers were all zero; all backing is already sealed until reboot. */
    status=cp_write_checked(0x808,2);
    if (!NT_SUCCESS(status)) return status;
    for (i=0;i<Q2Q_CP_SETUP_COUNT;++i) {
        status=cp_write_checked(CpSetup[i][0],CpSetup[i][1]);
        if (!NT_SUCCESS(status)) return status;
    }
    /* Match q2q's deassert sequence. ACK is observed, not required to fall:
     * neither q2q nor the audited source establishes that deassert contract. */
    if (CpUnhaltAttempted) return STATUS_INVALID_DEVICE_STATE;
    CpUnhaltAttempted=TRUE;
    status=cp_write_checked(0x3c45,0);
    if (NT_SUCCESS(status)) status=cp_write_checked(0x16,0);
    if (!NT_SUCCESS(status)) return status;
    status=cp_read(0x3c46,&value);
    if (NT_SUCCESS(status)) status=trace_val("cp_exec_gbif_ack_after_deassert_raw",STATUS_SUCCESS,value);
    if (!NT_SUCCESS(status)) return status;
    RtlZeroMemory(list,4096);
    for (i=0;i<Q2Q_CP_POWERUP_COUNT;++i) {
        list[4+i*2]=CpPowerupIndices[i];
        status=cp_read(CpPowerupIndices[i],&list[5+i*2]);
        if (!NT_SUCCESS(status)) return status;
    }
    list[3]=Q2Q_CP_LIST_LENGTH|(Q2Q_CP_LIST_OFFSET<<16);
    ring[0]=cp_packet(0x48,11);ring[1]=0x16f;ring[2]=3;ring[3]=0x20000000;
    ring[4]=ring[5]=ring[6]=0;ring[7]=2;
    ring[8]=(ULONG)CpIovas[2];ring[9]=(ULONG)(CpIovas[2]>>32);ring[10]=ring[11]=0;
    ring[12]=cp_packet(0x3d,3);ring[13]=(ULONG)(CpIovas[1]+64);
    ring[14]=(ULONG)((CpIovas[1]+64)>>32);ring[15]=Q2Q_CP_TOKEN;
    _DataSynchronizationBarrier();
    for (i=0;i<16;++i) {
        status=trace_val("cp_exec_stream_dword",STATUS_SUCCESS,ring[i]);
        if (!NT_SUCCESS(status)) return status;
    }
    status=trace_val("cp_exec_reglist_header",STATUS_SUCCESS,list[3]);
    if (!NT_SUCCESS(status)) return status;
    status=trace_val("cp_exec_result_before_token",STATUS_SUCCESS,result[16]);
    if (!NT_SUCCESS(status)) return status;
    status=cp_write_checked(0x804,(ULONG)CpIovas[1]);
    if (NT_SUCCESS(status)) status=cp_write_checked(0x805,(ULONG)(CpIovas[1]>>32));
    if (NT_SUCCESS(status)) status=cp_write_checked(0x802,0x20c);
    if (NT_SUCCESS(status)) status=cp_write_checked(0x800,(ULONG)CpIovas[0]);
    if (NT_SUCCESS(status)) status=cp_write_checked(0x801,(ULONG)(CpIovas[0]>>32));
    if (NT_SUCCESS(status)) status=cp_write_checked(0x830,(ULONG)GpuSqeIova);
    if (NT_SUCCESS(status)) status=cp_write_checked(0x831,(ULONG)(GpuSqeIova>>32));
    if (!NT_SUCCESS(status)) return status;
    if (!gpu_gx_live_gate() || !NT_SUCCESS(gpu_sqe_content_check())) return STATUS_DEVICE_NOT_READY;
    fence=READ_REGISTER_ULONG((volatile ULONG *)(RunCont+0x2ec4c));
    if (fence&1U) return STATUS_DEVICE_NOT_READY;
    status=trace_val("cp_exec_sqe_and_ring_config_verified",STATUS_SUCCESS,16);
    if (!NT_SUCCESS(status)) return status;
    start=now_us(NULL);
    status=cp_write_checked(0x808,1);
    if (!NT_SUCCESS(status)) return status;
    if (!gpu_gx_live_gate()) return STATUS_DEVICE_NOT_READY;
    now=now_us(NULL);
    if (now<start) return STATUS_DEVICE_PROTOCOL_ERROR;
    if (now-start>=100000U) return STATUS_IO_TIMEOUT;
    WRITE_REGISTER_ULONG((volatile ULONG *)(RunGfx+0x201c),16);
    _DataSynchronizationBarrier();
    fence=READ_REGISTER_ULONG((volatile ULONG *)(RunCont+0x2ec4c));
    /* No retry of an uncertain WPTR. The normal source checks this same drop bit. */
    status=(fence&1U)?STATUS_DEVICE_NOT_READY:STATUS_IO_TIMEOUT;
    for (i=0;i<101 && !(fence&1U);++i) {
        if (!gpu_gx_live_gate()) { status=STATUS_DEVICE_NOT_READY;break; }
        now=now_us(NULL);
        if (now<start) { status=STATUS_DEVICE_PROTOCOL_ERROR;break; }
        elapsed=now-start;
        if (elapsed>=100000U) break;
        _DataSynchronizationBarrier();
        token=result[16];shadow=result[0];
        if (!NT_SUCCESS(cp_read(0x806,&rptr)) || !NT_SUCCESS(cp_read(0x807,&wptr)) ||
                !NT_SUCCESS(cp_read(0x821,&fault)) || !NT_SUCCESS(cp_read(0x201,&irq))) {
            status=STATUS_DEVICE_NOT_READY;break;
        }
        now=now_us(NULL);
        if (now<start) { status=STATUS_DEVICE_PROTOCOL_ERROR;break; }
        elapsed=now-start;
        if (elapsed>=100000U) break;
        if (fault || irq) { status=STATUS_DEVICE_PROTOCOL_ERROR;break; }
        if (token==Q2Q_CP_TOKEN && shadow==16 && rptr==16 && wptr==16) {
            status=STATUS_SUCCESS;break;
        }
        if (delay_one_ms(NULL)!=0) { status=STATUS_DEVICE_NOT_READY;break; }
    }
    if (NT_SUCCESS(status)) {
        for (i=0;i<4096;++i)
            if (!((i<4)||(i>=64&&i<68)) && ((volatile UCHAR *)result)[i]!=0xa5) {
                status=STATUS_DEVICE_PROTOCOL_ERROR;break;
            }
        if (!gpu_gx_live_gate() || !NT_SUCCESS(gpu_sqe_content_check()))
            status=STATUS_DEVICE_NOT_READY;
    }
    logged=trace_val("cp_exec_token_raw",status,token);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_exec_shadow_rptr_raw",status,shadow);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_exec_rptr_raw",status,rptr);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_exec_wptr_raw",status,wptr);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_exec_fault_raw",status,fault);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_exec_irq_raw",status,irq);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_exec_fenced_write_status_raw",status,fence);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_exec_elapsed_us",status,elapsed);
    if (NT_SUCCESS(logged)) logged=trace_val("cp_exec_memory_completion_result",status,16);
    return NT_SUCCESS(status)?logged:status;
}
