/*
 * SSDT-Q2QBTUR - Bluetooth (QCA6490 over UART) for the Samsung SM-F926B (q2q).
 *
 * UR19 is QUPv3 wrapper 2 SE4 (global SE18), the high-speed UART of the Bluetooth chip. Resources from the
 * q2q device tree (/soc/qcom,qup_uart@890000 and bt_qca6490):
 *   0x00890000 / 0x4000  SE registers
 *   GSIV 0x26A           GIC SPI 0x24A + 32
 *   GpioInt 0x47         gpio71 (UART RX, wake), level high, pull-down
 *   GpioIo 0x41          gpio65 (BT_EN reset line)
 *   UartSerialBus        115200 baud, 8N1, hardware flow control (CTS gpio68, RTS gpio69, TX gpio70, RX gpio71)
 * HIDs: QCOM1A16 = Qualcomm UART (qcuart8350), QCOM1A6B = Qualcomm Bluetooth (qcbtfmuart_hsp8350).
 *
 * The connection uses UartSerialBus (descriptor revision 1): qcuart8350 rejects UartSerialBusV2.
 * The BT rails are enabled by the UEFI (shared with Wi-Fi) and are not ACPI resources here.
 * Also needs a PEP whose clock data enables gcc_qupv3_wrap2_s4_clk; with the stock BSP PEP the UART open
 * bug-checks (0x14E).
 */
DefinitionBlock ("", "SSDT", 2, "QCOMM ", "Q2QBTUR ", 1)
{
    External (\_SB.PSUB, StrObj)
    External (\_SB.PEP0, DeviceObj)
    External (\_SB.PMIC, DeviceObj)
    External (\_SB.GIO0, DeviceObj)
    Scope (\_SB)
    {
        Device (UR19)
        {
            Name (_HID, "QCOM1A16")
            Alias (\_SB.PSUB, _SUB)
            Name (_UID, 0x13)
            Name (_DEP, Package (0x01) { \_SB.PEP0 })
            Name (_CCA, Zero)
            Name (_STR, Unicode ("QUP_2_SE_4,4W,BT"))
            Method (_CRS, 0, Serialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x00890000, 0x00004000)
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, , , ) { 0x0000026A }
                    GpioInt (Level, ActiveHigh, Exclusive, PullDown, 0x0000, "\\_SB.GIO0", 0x00, ResourceConsumer, , ) { 0x0047 }
                })
                Return (RBUF)
            }
            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }
        Device (BTH0)
        {
            Name (_HID, "QCOM1A6B")
            Alias (\_SB.PSUB, _SUB)
            Name (_UID, Zero)
            Name (_DEP, Package (0x03) { \_SB.PEP0, \_SB.PMIC, \_SB.UR19 })
            Name (_CRS, ResourceTemplate ()
            {
                UartSerialBus (115200, DataBitsEight, StopBitsOne, 0xC0, LittleEndian, ParityTypeNone,
                    FlowControlHardware, 0x20, 0x20, "\\_SB.UR19", 0x00, ResourceConsumer, , )
                GpioIo (Exclusive, PullDown, 0x0000, 0x0000, IoRestrictionNone, "\\_SB.GIO0", 0x00, ResourceConsumer, , ) { 0x0041 }
            })
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }
    }
}
