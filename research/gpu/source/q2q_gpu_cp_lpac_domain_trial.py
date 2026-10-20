"""Fresh original TrEE/ZAP/CP trial with native CB0+CB2 global-domain retention."""
import argparse
from pathlib import Path
import types

import build_q2q_a660_gpu_cp_lpac_domain as candidate
import q2q_gpu_cp_lpac_state_trial as base

ROOT,prior = candidate.ROOT,base.prior
SOURCE = Path(__file__).resolve()
TREE_PACKAGE,TREE_SHA = base.TREE_PACKAGE,base.TREE_SHA
AUTHORIZATION = dict(base.AUTHORIZATION,
    phase='New claim only: original TrEE/ZAP19, unchanged CP and BO permissions; original native MMUI association/activation for current unused/default CB2/VM4, sharing the owned global range. No manual PTE/MMIO/SID/VMID/security or CB1/CB4 retarget.')


def values(record):
    result = prior.values(record)
    result['DisplayName'] = ('q2q original native CB0+CB2 globals and unchanged bounded CP token',1)
    return result


def analyse_rows(rows):
    result = base.analyse_rows(rows)
    native = [r for r in rows if r.get('stage') == 'gpu_cb2_native_domain_retained_result']
    verified = len(native)==1 and native[0].get('ntstatus')==0 and native[0].get('value')==2
    complete = result['first_cp_owned_memory_write_verified'] and verified
    result.update(native_cb0_cb2_global_domain_retained_verified=bool(verified),
        first_cp_owned_memory_write_verified=bool(complete),
        sqe_instruction_fetch_verified=bool(complete),gpu_dma_verified=bool(complete),
        gpu_command_submission_verified=bool(complete),
        cb2_limit='Current original native descriptor2/VM4 association, held range/power/backing; not direct hardware TTBR readback, SID routing, render fence or Draw proof.')
    return result


scope = dict(base.scope,SOURCE=SOURCE,candidate=candidate,values=values)
for name,function in list(base.scope.items()):
    if isinstance(function,types.FunctionType) and function.__globals__ is base.scope \
            and name not in ('values','capture'):
        bound = types.FunctionType(function.__code__,scope,name,function.__defaults__,function.__closure__)
        bound.__kwdefaults__ = function.__kwdefaults__
        scope[name] = bound
shared = dict(base.shared,scope=scope,candidate=candidate,SOURCE=SOURCE,analyse_rows=analyse_rows,
    OUTPUT_NAME='gpu-cp-lpac-domain-runtime',ROLLBACK_TAG='gpu-cp-lpac-domain-rollback',
    CAPTURE_SCOPE='Original native CB0+CB2 global ownership with unchanged CP/BOs/guard and original ZAP19, no manual routing/security or rendering capability')
capture_standard = types.FunctionType(base.capture_standard.__code__,shared,'capture_standard')
scope['capture'] = types.FunctionType(base.scope['capture'].__code__,
    dict(base.scope['capture'].__globals__,scope=scope,candidate=candidate,shared=shared,
         capture_standard=capture_standard),'capture')
old_constants = base.prepare.__code__.co_consts
if old_constants.count('gpu-cp-state-runtime/result.json') != 1:
    raise ValueError('Exact LPAC-observation predecessor path required')
prepare_code = base.prepare.__code__.replace(co_consts=tuple(
    'gpu-cp-lpac-state-runtime/result.json' if x == 'gpu-cp-state-runtime/result.json' else x for x in old_constants))
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
