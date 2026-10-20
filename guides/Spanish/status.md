# Estado del proyecto — 2026-10-20

Dispositivo: **Samsung Galaxy Z Fold3 SM-F926B / q2q**, Windows 11 ARM64.

| Función | Estado actual |
| --- | --- |
| Arranque de Windows | Funcional; alcanza OOBE y escritorio en la unidad de desarrollo. |
| Táctil de la pantalla principal | **Funciona**, confirmado por el operador. No incluye el táctil de la pantalla exterior. |
| Wi-Fi | **Funciona**, confirmado por el operador. No implica estabilidad prolongada ni suspensión verificadas. |
| Botones de encendido y volumen | **Funcionan**, confirmados por el operador. También se ha eliminado la antigua pausa de 10 segundos de UEFI. |
| Pantalla / GPU | Salida básica funcional. **Aceleración Adreno todavía en desarrollo**. |

Las confirmaciones de táctil, Wi-Fi y botones se reiteran por el usuario el
20 de octubre. No se presentan como una nueva prueba física de esta sesión.

El 19 de octubre se logró la primera escritura GPU real en un buffer propio
desde Windows: `CP_MEM_WRITE` cambió el token de `0x660bad00` a `0x660c0001`
en **225 µs**, con RPTR hardware, RPTR shadow y WPTR en 16. La cobertura de
globales LPAC mediante la asociación nativa CB2 resolvió el bloqueo observado
en el ensayo anterior con CB0 únicamente. GMU/HFI y la ruta original PIL/ZAP
también cuentan con evidencia runtime.

Esto acredita consumo de comandos y escritura DMA en ese ensayo. **No acredita
todavía aceleración Direct3D/DXGI, Draw/readback, fence de render, entrega de
IRQ ni estabilidad**. El siguiente ensayo, con el paquete original
`CP_SET_SECURE_MODE(0)`, quedó aplicado y con arranque solicitado, pero sin
resultado runtime recogido en la evidencia revisada. Después consta la
restauración verificada de la UEFI funcional, sin reinicio en esa restauración.

Se incluyen fuentes y parches UEFI, driver táctil ARM64, corrección PEP LMH,
dependencias fijadas por versión/hash y configuración de escalado al 200%.
La reproducción del escalado en una instalación limpia sigue pendiente.
Bluetooth, lápiz, cámaras, sensores, audio y suspensión no se dan por validados.

Detalles y límites: [STATE.md](../../STATE.md).
Las guías de instalación y recuperación están incompletas; todavía no hay
una publicación de instalación validada de principio a fin.
