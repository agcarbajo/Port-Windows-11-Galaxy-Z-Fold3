# A660 GPU research snapshot — 2026-10-20

These source excerpts document the latest development work. They are **not a
standalone build**, an installable accelerated driver or part of the offline
installer. They depend on the private development workspace's native-provider
adapter, pinned source audits, build pipeline and fresh per-trial evidence.
Missing dependencies must not be replaced with guessed hardware resources.

The functional UEFI/touch/WLAN packages remain separate. Current acceptance
status is maintained in [STATE.md](../../STATE.md).

## Verified Windows results

1. Original GMU firmware executes and answers HFI version requests
   (`0x30010008`). Captured performance/bandwidth tables are acknowledged;
   core start and subsequent version replies complete. Accepted performance
   votes alone are not measured GPU clocks.
2. Original MMUI callbacks create and activate retained owned memory domains.
   Backing, associations, provider and power references remain held after
   activation or an uncertain failure. Original PIL/ZAP authentication and
   a fresh later HFI response complete in the same tested sequence.
3. Source and measured SQE analysis identify an embedded LPAC bootstrap. Its
   observed base equals the owned SQE base plus `0x7e60`. A CB0-only experiment
   programs LPAC but times out without producing the CP result token.
4. Sharing the owned global range through the original native CB2 / VM4
   association produces the first completed `CP_MEM_WRITE`: new token
   `0x660c0001`, hardware/shadow RPTR 16 and WPTR 16, at 225 µs.

The last result comes from the 2026-10-19 trial identified in the local
evidence store as `recovery-return-20261019T064703235799Z`, result
`gpu-cp-lpac-domain-runtime/result.json`. Its journals were copied and hashed;
normal return, baseline restoration and consumed-service cleanup were
recorded. Raw captures contain device/host identifiers and are not included
in this public snapshot. These are reported local results, not a claim that
a fresh clone contains enough evidence or dependencies to rerun the trial.

## Source map

| Excerpt | Purpose |
| --- | --- |
| `q2q_cp_init_contract.py` | Source-pinned A660 CP setup / power-up list contract |
| `q2q_gpu_cp_preflight.inc` | Initial CP state gate |
| `q2q_gpu_cp_prepare.inc` | Owned-buffer preparation and retained CPU contents |
| `q2q_gpu_cp_bootstrap.inc` | Bounded CP submission, completion and canary checks |
| `q2q_gpu_cp_state.inc` | Post-guard status observations |
| `q2q_gpu_cp_lpac_state.inc` | Initial and post-guard LPAC observations |
| `q2q_gpu_lpac_native.inc` | Native CB2 association, activation and lifetime checks |
| `q2q_gpu_cp_lpac_domain_trial.py` | Trial analysis requiring both native coverage and CP completion |
| `q2q_gpu_cp_secure_packet_trial.py` | Subsequent original secure-packet experiment and analysis |

Original source comments and license notices are preserved. Exact SHA-256
values are in [source-manifest.json](source-manifest.json). The captured
source is the current workspace snapshot; historical trial build manifests
remain the authority for the exact code used in each historical run.

## Pending acceptance

The next secure-packet trial was prepared/applied with a new claim and token,
but has no collected runtime result in the evidence reviewed for publication.
Its expected token or host fixtures must not be presented as device output.

Still unverified: observed secure-state transition, render/cache-flush fence,
IRQ delivery, Draw/readback, hardware DXGI/Direct3D exposure and stability.
The installer does not install these research excerpts as an acceleration
driver. The latest recorded action restored the functional UEFI baseline;
there was no fresh device boot or test during publication.
