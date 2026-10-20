"""Source-pinned A660v2 GX/CP setup and power-up list for an owned CP experiment."""
import hashlib
import json
from pathlib import Path
import re

import audit_q2q_cp_native as native
from capture_offline_windows import sha256

ROOT, AUDIT = native.ROOT, native.AUDIT


def contract():
    manifest = json.loads((AUDIT/'manifest.json').read_text())
    if manifest['commit'] != native.COMMIT:
        raise ValueError('Only audited q2q source may define this initialization')
    for name,entry in manifest['files'].items():
        if sha256(AUDIT/entry['file']) != entry['sha256']:
            raise ValueError('Frozen q2q source changed: '+name)
    header = (AUDIT/'a6xx_reg.h').read_text()
    source = (AUDIT/'adreno_a6xx.c').read_text()
    cores = (AUDIT/'adreno-gpulist.h').read_text()

    def index(name):
        parts = name.strip().split('+')
        match = re.search(r'^#define\s+'+parts[0].strip()+r'\s+(0[xX][0-9a-fA-F]+)\s*$',header,re.M)
        if not match:
            raise ValueError('Unknown source register '+name)
        value = int(match[1],16)+(int(parts[1]) if len(parts)==2 else 0)
        if len(parts)>2 or value*4 >= 0x40000:
            raise ValueError('Register outside owned GX aperture')
        return value

    def array(text,name):
        match = re.search(r'\b'+name+r'\[\]\s*=\s*\{(.*?)\n\};',text,re.S)
        if not match:
            raise ValueError('Exact source array missing '+name)
        return re.sub(r'/\*.*?\*/','',match[1],flags=re.S)

    core = cores.split('static const struct adreno_a6xx_core adreno_gpu_core_a660v2 = {',1)[1].split('\n};',1)[0]
    if any(token not in core for token in ('.highest_bank_bit = 16,','.prim_fifo_threshold = 0x00300000,',
            '.disable_tseskip = true,','.protected_regs = a660_protected_regs,','ADRENO_APRIV')):
        raise ValueError('A660v2 core contract changed')
    setup = []

    def add(name,value):
        setup.append((index('A6XX_'+name),value))

    for name in ('CP','VSC','GRAS','RB','PC','HLSQ','VFD','VPC','UCHE','SP','TPL1'):
        add(name+'_ADDR_MODE_CNTL',1)
    for name,value in re.findall(r'\{\s*(A6XX_\w+)\s*,\s*(0x[0-9a-fA-F]+)\s*\}',array(cores,'a650_gbif_regs')):
        setup.append((index(name),int(value,16)))
    add('RBBM_PERFCTR_GPU_BUSY_MASKED',0xffffffff)
    for name,value in (
            ('UCHE_WRITE_RANGE_MAX_LO',0xffffffc0),('UCHE_WRITE_RANGE_MAX_HI',0x1ffff),
            ('UCHE_TRAP_BASE_LO',0xfffff000),('UCHE_TRAP_BASE_HI',0x1ffff),
            ('UCHE_WRITE_THRU_BASE_LO',0xfffff000),('UCHE_WRITE_THRU_BASE_HI',0x1ffff),
            ('UCHE_FILTER_CNTL',0x804),('UCHE_CACHE_WAYS',4),
            ('CP_ROQ_THRESHOLDS_2',0x02000140),('CP_ROQ_THRESHOLDS_1',0x8040362c),
            ('CP_LPAC_PROG_FIFO_SIZE',32),('CP_MEM_POOL_SIZE',128),
            ('PC_DBG_ECO_CNTL',0x00300200),('CP_AHB_CNTL',1),('RBBM_PERFCTR_CNTL',1),
            ('RB_NC_MODE_CNTL',0x816),('TPL1_NC_MODE_CNTL',6),('SP_NC_MODE_CNTL',0x26),
            ('UCHE_MODE_CNTL',0x600000),('RBBM_INTERFACE_HANG_INT_CNTL',0x40cfffff),
            ('UCHE_CLIENT_PF',1)):
        add(name,value)
    for i,value in enumerate((0,0x3fe05ff4,0x3fa0ebee,0x3f5193ed,0x3f0243f0)):
        add('TPL1_BICUBIC_WEIGHTS_TABLE_'+str(i),value)
    add('CP_PROTECT_CNTL',0xb)
    protection = re.findall(r'\{\s*(A6XX_CP_PROTECT_REG\s*\+\s*\d+)\s*,\s*(0x\w+)\s*,\s*(0x\w+)\s*,\s*([01])\s*\}',
        array(cores,'a660_protected_regs'))
    if len(protection)!=42:
        raise ValueError('Exact A660 protection entries required, not disabled protection')
    for name,start,end,noaccess in protection:
        start,end,noaccess = int(start,16),int(end,16),int(noaccess)
        setup.append((index(name),(start&0x3ffff)|(((end-start)&0x1fff)<<18)|(noaccess<<31)))
    for name,value in (('CP_CHICKEN_DBG',1),('RBBM_GBIF_CLIENT_QOS_CNTL',0),
                       ('UCHE_CMDQ_CONFIG',0x66906),('CP_APRIV_CNTL',0x6e)):
        add(name,value)
    lists = []
    for name in ('a6xx_ifpc_pwrup_reglist','a650_ifpc_pwrup_reglist','a6xx_pwrup_reglist','a650_pwrup_reglist'):
        lists.append([index(v) for v in array(source,name).split(',') if v.strip()])
    offset = 2*(len(lists[0])+len(lists[1]))
    registers = [v for group in lists for v in group]
    registers += [index('A6XX_RBBM_GBIF_CLIENT_QOS_CNTL'),index('A6XX_RBBM_PERFCTR_CNTL')]
    if 16+len(registers)*8 > 4096:
        raise ValueError('Reglist must fit exactly within its own page')
    return dict(commit=native.COMMIT,source_manifest_sha256=sha256(AUDIT/'manifest.json'),
        native=native.native_contract(),setup=setup,powerup_indices=registers,
        list_offset_dwords=offset,list_length_dwords=len(registers)*2,
        chipid=0x06060001,ddr_type=8,min_access=32,ubwc_mode=4,highest_bank_bit=16,
        ring_bytes=32768,ring_control=0x20c,me_init_mask=0x16f,
        limits=['Owned CP-only test, not complete rendering initialization or WDDM capabilities.',
                'No optional LLCC override, HWCG, preemption, GMU counter, TSB or secure-mode programming.',
                'No IRQ unmask, manual PTE, physical DMA address or ZAP address supplied.',
                'GBIF ACK0 is not a source-mandated condition after zero deassert.',
                'A new native ZAP claim is required separately before a secure transition/Draw.'])


def header_text():
    data = contract()
    pairs = ',\n'.join('{'+hex(r)+'U,'+hex(v)+'U}' for r,v in data['setup'])
    indices = ','.join(hex(r)+'U' for r in data['powerup_indices'])
    return ('/* SPDX-License-Identifier: GPL-2.0-only\n'
        ' * Derived from local Samsung q2q 754466; original source/license retained in audit snapshot.\n */\n'
        'static const ULONG CpSetup[][2]={\n'+pairs+'\n};\n'
        'static const ULONG CpPowerupIndices[]={'+indices+'};\n'
        '#define Q2Q_CP_LIST_OFFSET '+str(data['list_offset_dwords'])+'U\n'
        '#define Q2Q_CP_LIST_LENGTH '+str(data['list_length_dwords'])+'U\n'
        '#define Q2Q_CP_SETUP_COUNT '+str(len(data['setup']))+'U\n'
        '#define Q2Q_CP_POWERUP_COUNT '+str(len(data['powerup_indices']))+'U\n')
