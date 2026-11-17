# Estado del proyecto

[🏠 Inicio](../../README.md) | [English](../English/status.md) | **Español**

- Dispositivo: **Samsung Galaxy Z Fold3 — SM-F926B / q2q**
- Última actualización: **2026-11-17**
- Releases: todavía ninguna (versión de desarrollo)

✅ Funciona · ⚠️ Parcial · ❌ No funciona · ❔ Sin probar

Los resultados marcados con ¹ son de la unidad de desarrollo y necesitan el paquete del driver
de la GPU y cambios de UEFI/ACPI que aún no están publicados.

## Resumen

| Función | Estado | Notas |
| --- | :---: | --- |
| Arranque / UEFI | ✅ | UEFI Mu-Silicium en `boot`, TWRP se conserva en `recovery` |
| Pantalla interior | ✅¹ | 120 Hz, 60/120 Hz seleccionable, brillo |
| Pantalla exterior | ⚠️¹ | 120 Hz y brillo; a 60 Hz falla si es la única pantalla activa |
| Las dos pantallas | ✅¹ | Independientes (extender, solo interior, solo exterior) |
| GPU (Adreno 660) | ✅¹ | Driver WDDM propio, GPU desde el primer fotograma, visible en el Administrador de tareas |
| Transparencias / DWM | ✅¹ | Acrílico / Mica compuestos en la GPU |
| Navegador | ✅¹ | Composición por GPU y WebGL en Edge / Chrome (~85–95 fps) |
| Direct3D 11 nativo | ✅¹ | Nivel de funciones 10_1, ARM64 y x64 |
| Direct3D 9–12 | ✅¹ | DXVK 2.7.1 / vkd3d-proton 3.1, instalados por juego |
| Vulkan | ✅¹ | Mesa turnip, cargador Khronos de sistema |
| OpenGL | ⚠️¹ | OpenGL 4.6 con zink; más lento que Vulkan en ventanas |
| Apps y juegos x64 | ✅¹ | Emulación x64 sobre la misma GPU (ARM64EC); sin x86 de 32 bits |
| Táctil interior | ✅ | Driver ARM64 propio (STM FTS por SPI) |
| Táctil exterior | ❌ | El controlador I2C arranca, pero las transferencias caducan |
| Wi-Fi | ✅ | |
| Bluetooth | ⚠️¹ | Radio y búsqueda BLE/clásica; emparejamiento sin verificar |
| Batería | ⚠️¹ | Porcentaje visible; las dos baterías se ven como una (un solo medidor) |
| Carga | ⚠️ | Mantiene la configuración del firmware; Windows no la gestiona |
| Botones | ✅ | Encendido, subir y bajar volumen |
| Almacenamiento (UFS) | ✅ | |
| Sensor de bisagra | ❌ | Sin detección de plegado ni rotación |
| Otros sensores | ❔ | |
| Audio | ❔ | |
| USB | ❔ | |
| Cámaras | ❔ | |
| Datos móviles | ❔ | |
| Huella / S Pen | ❔ | |
| Suspensión | ❔ | Hibernación desactivada |

Medido en la unidad de desarrollo: SuperTuxKart 1.5 a 1280×720 — OpenGL 98,6 fps (ARM64) /
75,5 fps (x64); Direct3D 9 con DXVK 113 fps (ARM64) / 112 fps (x64). Arrastre de ventanas
~8,3 ms por fotograma (120 fps) con las dos pantallas activas.

## Problemas conocidos

- La pantalla exterior a 60 Hz deja de actualizarse cuando es la única pantalla activa.
- OpenGL (zink) es bastante más lento que Vulkan / DXVK en ventanas pequeñas.
- El táctil exterior todavía no funciona.
- Los drivers están firmados con un certificado de desarrollo: hay que mantener la firma de prueba activada.

El detalle por subsistema está en la [versión en inglés](../English/status.md).
