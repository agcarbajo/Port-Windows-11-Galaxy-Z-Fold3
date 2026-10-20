# Estado del proyecto — 2026-10-20

[🏠 Inicio](../../README.md) | [English](../English/status.md) | **Español**

Dispositivo: **Samsung Galaxy Z Fold3 SM-F926B / q2q**, Windows 11 ARM64.

Drivers y UEFI: versión de desarrollo, todavía sin release numerada.
La base UEFI funcional se restauró el 19 de octubre.

## Estado resumido

✅ Funciona · ⚠️ Parcial / en desarrollo · ❔ Sin verificar

| Función | Notas | Estado |
| --- | --- | :---: |
| 🪟 Arranque de Windows | Alcanza OOBE / escritorio en la unidad de desarrollo | ✅ |
| 🖥️ Pantalla | Salida básica funcional; plegado y cambio de panel sin verificar | ⚠️ |
| 🎆 GPU | Comandos reales completados; aceleración Direct3D / DXGI en desarrollo | ⚠️ |
| 👆 Táctil principal | Funcionamiento confirmado por el operador | ✅ |
| 👆 Táctil exterior | Controlador independiente; sin verificar | ❔ |
| 🛜 Wi-Fi | Conectividad confirmada por el operador | ✅ |
| ⌨️ Botones laterales | Funcionan encendido, subir y bajar volumen | ✅ |
| 💾 Almacenamiento | Windows arranca desde UFS; estrés y E/S nativa en todos los LUN sin verificar | ⚠️ |
| 🔊 Audio | Audio Windows sin verificar; los altavoces internos de esta unidad tienen una avería física | ❔ |
| 🔋 Batería | Lectura de batería y gestión de energía sin verificar | ❔ |
| 🔌 Carga | Comportamiento en Windows sin verificar | ❔ |
| 🪵 USB | Modos host / OTG y dispositivo en Windows sin verificar | ❔ |
| 🔵 Bluetooth | Sin verificar | ❔ |
| 🖊️ Lápiz | Sin verificar | ❔ |
| 📷 Cámaras | Sin verificar | ❔ |
| 🧭 Sensores | Bisagra, rotación y demás sensores sin verificar | ❔ |
| 🔒 Huella | Sin verificar | ❔ |
| 💤 Suspensión / reanudación | Sin verificar | ❔ |

Las confirmaciones de táctil, Wi-Fi y botones se reiteran por el usuario el
20 de octubre. No se presentan como una nueva prueba física de esta sesión.

## Estado detallado

### 🎆 Pantalla y GPU

| Función | Notas | Estado |
| --- | --- | :---: |
| Salida básica de pantalla | Salida de Windows utilizable | ✅ |
| Firmware GMU / HFI | Ejecución real, respuestas de versión, ACK de tablas y arranque del core verificados | ✅ |
| Comandos / DMA GPU | Primer CP write a buffer propio completado en **225 µs** el 19 de octubre | ✅ |
| Direct3D / DXGI por hardware | Sin verificar; no se anuncian capacidades WDDM aceleradas | ⚠️ |
| Draw / readback | Sin verificar | ❔ |
| Fence de render / entrega de IRQ | Sin verificar | ❔ |
| Brillo / cambio de panel | Sin verificar | ❔ |
| Estabilidad prolongada de GPU | Sin verificar | ❔ |

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

### 👆 Táctil

| Función | Notas | Estado |
| --- | --- | :---: |
| Táctil principal | Entrada física confirmada; driver STM SPI ARM64 incluido | ✅ |
| Táctil exterior | No incluido en la confirmación del táctil principal | ❔ |
| Límites multitáctiles / gestos | Sin resultado de aceptación específico | ❔ |

### 🛜 Wi-Fi

| Función | Notas | Estado |
| --- | --- | :---: |
| Conectividad | Confirmada; inicialización PCIe/WLAN en UEFI y paquete Qualcomm fijado incluidos | ✅ |
| 2,4 GHz / 5 GHz por separado | Sin resultado de aceptación por banda | ❔ |
| Wi-Fi Direct / Miracast | Sin verificar | ❔ |
| Rendimiento / estabilidad tras suspensión | Sin verificar | ❔ |

### ⌨️ Botones laterales

| Función | Notas | Estado |
| --- | --- | :---: |
| Subir volumen | Confirmado por el operador | ✅ |
| Bajar volumen | Confirmado por el operador | ✅ |
| Encendido | Confirmado por el operador | ✅ |

También está confirmada la eliminación de la antigua pausa de 10 segundos de UEFI.

### 🔧 Estado de la instalación

Se incluyen fuentes y parches UEFI, driver táctil ARM64, corrección PEP LMH,
dependencias fijadas por versión/hash y configuración de escalado al 200%.
La reproducción del escalado en una instalación limpia sigue pendiente.
Bluetooth, lápiz, cámaras, sensores, audio y suspensión no se dan por validados.

Las guías de instalación y recuperación están incompletas; todavía no hay
una publicación de instalación validada de principio a fin.

Fuentes experimentales y límites de la evidencia: [investigación GPU](../../research/gpu/README.md).
Organización basada en el [estado del proyecto Xiaomi Pad 5](https://github.com/erdilS/Port-Windows-11-Xiaomi-Pad-5/blob/main/guide/English/status.md);
los resultados de compatibilidad anteriores corresponden únicamente al Fold3.
