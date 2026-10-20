"""Original secure-mode0 packet completion, not an assumed security-state readback."""
import argparse
from pathlib import Path
import types

import build_q2q_a660_gpu_cp_secure_packet as candidate
import q2q_gpu_cp_lpac_domain_trial as base
import q2q_cp_init_contract as hardware

ROOT,prior = candidate.ROOT,base.prior
SOURCE = Path(__file__).resolve()
TREE_PACKAGE,TREE_SHA = base.TREE_PACKAGE,base.TREE_SHA
AUTHORIZATION = dict(base.AUTHORIZATION,
    phase='New original TrEE/ZAP19 claim plus source CP_SET_SECURE_MODE0 packet after ME_INIT, before new private token. No manual secure control or opaque PIL address.')


def values(record):
    result = prior.values(record)
    result['DisplayName'] = ('q2q original secure-mode0 packet and new private GPU token, not Draw',1)
    return result


def analyse_rows(rows):
    result = base.analyse_rows(rows)
    stages = {r.get('stage'):r for r in rows}
    complete = all(result.get(n) is True for n in (
        'cp_typed_buffers_cpu_retained_verified','original_zap_with_cb0_and_post_hfi_verified',
        'native_cb0_cb2_global_domain_retained_verified','fifo_after_sqe_qualification_verified',
        'initial_lpac_zero_reads_verified','post_guard_nine_status_reads_verified'))
    expected = dict(cp_exec_attempted=1,cp_exec_sqe_and_ring_config_verified=18,
        cp_exec_result_before_token=0x660bad00,cp_exec_token_raw=0x660c0002,
        cp_exec_shadow_rptr_raw=18,cp_exec_rptr_raw=18,cp_exec_wptr_raw=18,
        cp_exec_fault_raw=0,cp_exec_irq_raw=0,cp_exec_memory_completion_result=18,
        cp_after_interrupt_status_raw=0,cp_after_protect_status_raw=0)
    contract = hardware.contract()
    expected['cp_exec_reglist_header'] = contract['list_length_dwords']|(contract['list_offset_dwords']<<16)
    complete = complete and all(stages.get(n,{}).get('ntstatus')==0 and
        stages[n].get('value')==v and sum(r.get('stage')==n for r in rows)==1 for n,v in expected.items())
    stream_rows = [r for r in rows if r.get('stage')=='cp_exec_stream_dword']
    reglist = stages.get('cp_reglist_rw_nx_iova',{}).get('value',0)
    target = stages.get('cp_result_rw_nx_iova',{}).get('value',0)+64
    stream = [0x70c8000b,0x16f,3,0x20000000,0,0,0,2,reglist&0xffffffff,reglist>>32,0,0,
              0x70e60001,0,0x703d8003,target&0xffffffff,target>>32,0x660c0002]
    complete = complete and len(stream_rows)==18 and all(r.get('ntstatus')==0 for r in stream_rows) and \
        [r.get('value') for r in stream_rows]==stream
    elapsed = [r for r in rows if r.get('stage')=='cp_exec_elapsed_us']
    fence = [r for r in rows if r.get('stage')=='cp_exec_fenced_write_status_raw']
    complete = complete and len(elapsed)==len(fence)==1 and elapsed[0].get('ntstatus')==0 and \
        0<=elapsed[0].get('value',-1)<100000 and fence[0].get('ntstatus')==0 and \
        isinstance(fence[0].get('value'),int) and fence[0]['value']>=0 and not (fence[0]['value']&1)
    result.update(first_cp_owned_memory_write_verified=bool(complete),gpu_dma_verified=bool(complete),
        sqe_instruction_fetch_verified=bool(complete),gpu_command_submission_verified=bool(complete),
        original_zap_secure_mode0_command_completed=bool(complete),
        secure_transition_verified=False,gpu_render_fence_verified=False,
        secure_packet_limit='Exact18-DWORD ME_INIT/SET_SECURE_MODE0/MEM_WRITE and fresh token/RPTR. No initial secure mode sampled or secure residency readback, so observed state transition remains unverified. No CPU SECVID write, render fence, Draw or DXGI.')
    return result


scope = dict(base.scope,SOURCE=SOURCE,candidate=candidate,values=values)
for name,function in list(base.scope.items()):
    if isinstance(function,types.FunctionType) and function.__globals__ is base.scope \
            and name not in ('values','capture'):
        bound = types.FunctionType(function.__code__,scope,name,function.__defaults__,function.__closure__)
        bound.__kwdefaults__ = function.__kwdefaults__
        scope[name] = bound
shared = dict(base.shared,scope=scope,candidate=candidate,SOURCE=SOURCE,analyse_rows=analyse_rows,
    OUTPUT_NAME='gpu-cp-secure-packet-runtime',ROLLBACK_TAG='gpu-cp-secure-packet-rollback',
    CAPTURE_SCOPE='Original ZAP19/new claim, native CB0+CB2, source secure-mode0 packet plus fresh private token; no direct secure-state readback/render/Draw')
capture_standard = types.FunctionType(base.capture_standard.__code__,shared,'capture_standard')
scope['capture'] = types.FunctionType(base.scope['capture'].__code__,
    dict(base.scope['capture'].__globals__,scope=scope,candidate=candidate,shared=shared,
         capture_standard=capture_standard),'capture')
old_constants = base.prepare.__code__.co_consts
if old_constants.count('gpu-cp-lpac-state-runtime/result.json') != 1:
    raise ValueError('Exact native-CB2-completion predecessor required')
prepare_code = base.prepare.__code__.replace(co_consts=tuple(
    'gpu-cp-lpac-domain-runtime/result.json' if x=='gpu-cp-lpac-state-runtime/result.json' else x for x in old_constants))
prepare = types.FunctionType(prepare_code,dict(base.prepare.__globals__,
    scope=scope,SOURCE=SOURCE,candidate=candidate,AUTHORIZATION=AUTHORIZATION),'prepare')
dispatch = dict(base.apply.__globals__,scope=scope,candidate=candidate,AUTHORIZATION=AUTHORIZATION)
apply = types.FunctionType(base.apply.__code__,dispatch,'apply')
reboot = types.FunctionType(base.reboot.__code__,dispatch,'reboot')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('prepare','plan','apply','reboot','capture','cleanup'))
    for name in ('target','driver-build','previous','restoration','cleanup','capture-dir'):
        parser.add_argument('--'+name,type=Path)
    args = parser.parse_args()
    if args.action == 'prepare':
        if not all((args.driver_build,args.previous,args.restoration,args.cleanup)):
            parser.error('Fresh build/predecessor/restoration/cleanup required')
        prepare(args.driver_build,args.previous,args.restoration,args.cleanup)
    elif args.target is None:
        parser.error('Explicit target required')
    elif args.action == 'apply':
        apply(args.target)
    elif args.action == 'reboot':
        reboot(args.target)
    elif args.action == 'cleanup':
        if args.capture_dir is None:
            parser.error('Fresh postboot capture required')
        scope['cleanup'](args.target,args.capture_dir.resolve())
    else:
        scope[args.action](args.target)
