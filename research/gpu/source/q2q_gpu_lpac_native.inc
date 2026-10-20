/* SPDX-License-Identifier: GPL-2.0-only
 * Share this owner's existing VM4 global domain with original LPAC descriptor2.
 * The original provider alone programs its CB; never retarget CB1 or CB4.
 */
static NTSTATUS lpac_association_state(Q2Q_MMUI_MEMORY *owner,ULONG expected,BOOLEAN active)
{
    UCHAR association[0x28];
    PVOID domain=NULL,current[2]={NULL,NULL},descriptor=NULL;
    NTSTATUS status;
    if (!owner->LpacAssociation || !owner->Range) return STATUS_INVALID_DEVICE_STATE;
    status=copy_virtual(owner->LpacAssociation,association,sizeof(association));
    if (!NT_SUCCESS(status)) return status;
    descriptor=*(UNALIGNED PVOID *)(association+8);
    if (*(UNALIGNED PVOID *)association!=owner->Range ||
            descriptor!=(PUCHAR)owner->Interface.Header.Context+0x10c8+56*2 ||
            *(UNALIGNED ULONG *)(association+0x10)!=0 ||
            *(UNALIGNED ULONG *)(association+0x14)!=expected ||
            *(UNALIGNED ULONG *)(association+0x18)!=0x05060708)
        return STATUS_DEVICE_PROTOCOL_ERROR;
    status=copy_virtual((PUCHAR)owner->Range+0x18,&domain,sizeof(domain));
    if (NT_SUCCESS(status)) status=copy_virtual((PUCHAR)descriptor+0x18,current,sizeof(current));
    if (!NT_SUCCESS(status)) return status;
    if (!domain || (active ? ((current[0]==domain)==(current[1]==domain)) :
                    (current[0]==domain || current[1]==domain)))
        return STATUS_DEVICE_PROTOCOL_ERROR;
    return STATUS_SUCCESS;
}

static NTSTATUS lpac_default_roots(PUCHAR module,PVOID descriptor)
{
    PVOID current[2]={NULL,NULL},singleton=NULL,native_default=NULL;
    NTSTATUS status;
    status=copy_virtual((PUCHAR)descriptor+0x18,current,sizeof(current));
    if (!NT_SUCCESS(status)) return status;
    if (!current[0] && !current[1]) return STATUS_SUCCESS;
    status=copy_virtual(module+0x60330,&singleton,sizeof(singleton));
    if (!NT_SUCCESS(status)) return status;
    if (!singleton) return STATUS_DEVICE_PROTOCOL_ERROR;
    status=copy_virtual((PUCHAR)singleton+8,&native_default,sizeof(native_default));
    if (!NT_SUCCESS(status)) return status;
    return native_default && current[0]==native_default && current[1]==native_default ?
        STATUS_SUCCESS:STATUS_DEVICE_BUSY;
}

static NTSTATUS lpac_prepare(Q2Q_MMUI_MEMORY *owner,PUCHAR module,PVOID descriptor)
{
    UCHAR raw[56];
    NTSTATUS status;
    if (owner->LpacAssociation || owner->Published || owner->Released || owner->Poisoned)
        return STATUS_INVALID_DEVICE_STATE;
    if (descriptor!=(PUCHAR)owner->Interface.Header.Context+0x10c8+56*2)
        return STATUS_DEVICE_PROTOCOL_ERROR;
    status=copy_virtual(descriptor,raw,sizeof(raw));
    if (!NT_SUCCESS(status)) return status;
    if (raw[0]!=2 || *(UNALIGNED ULONG *)(raw+4)!=4 ||
            *(UNALIGNED ULONG *)(raw+8)!=3 ||
            *(UNALIGNED PVOID *)(raw+16)!=owner->Interface.Header.Context)
        return STATUS_DEVICE_PROTOCOL_ERROR;
    status=lpac_default_roots(module,descriptor);
    if (!NT_SUCCESS(status)) return status;
    owner->LpacAssociation=((MMUI_ASSOCIATE)callback(owner,0xa0))(owner->Range,descriptor,0);
    if (!owner->LpacAssociation) {
        owner->Poisoned=TRUE;
        return STATUS_UNSUCCESSFUL;
    }
    status=lpac_association_state(owner,1,FALSE);
    if (!NT_SUCCESS(status)) owner->Poisoned=TRUE;
    return status;
}

static NTSTATUS lpac_activate(Q2Q_MMUI_MEMORY *owner)
{
    MMUI_ASSOCIATION_STATE activate=NULL;
    PUCHAR module=NULL;
    ULONG count=0;
    NTSTATUS status;
    if (!owner->Published || owner->Released || owner->Poisoned ||
            !owner->NativeAssociationActive || owner->LpacActive)
        return STATUS_INVALID_DEVICE_STATE;
    status=Q2qGpuBoPowerCheck(owner,&count);
    if (NT_SUCCESS(status)) status=validate_provider(&owner->Interface,&module);
    if (NT_SUCCESS(status)) status=lpac_default_roots(module,
        (PUCHAR)owner->Interface.Header.Context+0x10c8+56*2);
    if (NT_SUCCESS(status)) status=lpac_association_state(owner,1,FALSE);
    if (NT_SUCCESS(status)) status=association_callback(owner,0xb0,0x37170,&activate);
    if (!NT_SUCCESS(status)) return status;
    /* A failure can follow partial native hardware programming. */
    owner->LpacActive=TRUE;
    status=activate(owner->LpacAssociation);
    if (NT_SUCCESS(status)) status=lpac_association_state(owner,0,TRUE);
    if (NT_SUCCESS(status)) status=Q2qGpuBoPowerCheck(owner,&count);
    if (!NT_SUCCESS(status)) owner->Poisoned=TRUE;
    return status;
}

static NTSTATUS lpac_disassociate_before_publication(Q2Q_MMUI_MEMORY *owner)
{
    NTSTATUS status;
    if (!owner->LpacAssociation) return STATUS_SUCCESS;
    if (owner->Published || owner->LpacActive || owner->Released || owner->Poisoned)
        return STATUS_INVALID_DEVICE_STATE;
    status=((MMUI_DISASSOCIATE)callback(owner,0xa8))(owner->LpacAssociation);
    if (!NT_SUCCESS(status)) {
        owner->Poisoned=TRUE;
        return status;
    }
    owner->LpacAssociation=NULL;
    return STATUS_SUCCESS;
}

NTSTATUS Q2qGpuBoLpacAssociationCheck(Q2Q_MMUI_MEMORY *owner)
{
    NTSTATUS status;
    if (!owner || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    if (!owner->LpacActive || owner->Poisoned) return STATUS_INVALID_DEVICE_STATE;
    status=lpac_association_state(owner,0,TRUE);
    if (!NT_SUCCESS(status)) owner->Poisoned=TRUE;
    return status;
}

NTSTATUS Q2qGpuBoAssociationCheck(Q2Q_MMUI_MEMORY *owner)
{
    NTSTATUS status=Q2qGpuBoPrimaryAssociationCheck(owner);
    return NT_SUCCESS(status)?Q2qGpuBoLpacAssociationCheck(owner):status;
}
