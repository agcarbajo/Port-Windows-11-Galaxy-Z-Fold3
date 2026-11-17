/*
 * SSDT-Q2QBATD - battery for the Samsung SM-F926B (q2q): three QUP I2C controllers and the battery device
 * \_SB.BAT0 (HID Q2QC0B01) served by the own driver drivers/src/q2qbatt.
 *   I2C8  QUP_0_SE_7  0x0099C000  GSIV 0x280   MAX77705 fuel gauge 0x36 + charger 0x69
 *   I2C2  QUP_0_SE_1  0x00984000  GSIV 0x27A   S2ASL01 main-pack limiter 0x38
 *   IC10  QUP_1_SE_1  0x00A84000  GSIV 0x182   S2ASL01 sub-pack limiter 0x39
 * BAT0 lists the four I2C targets in the order the driver expects. 400 kHz, no interrupts (the driver polls)
 * and no writes.
 */
DefinitionBlock ("", "SSDT", 2, "QCOMM ", "Q2QBATD ", 1)
{
    External (\_SB.PSUB, StrObj)
    External (\_SB.PEP0, DeviceObj)
    Scope (\_SB)
    {
        Device (I2C8)
        {
            Name (_HID, "QCOM1A10")
            Alias (\_SB.PSUB, _SUB)
            Name (_UID, 0x08)
            Name (_DEP, Package (0x01) { \_SB.PEP0 })
            Name (_CCA, Zero)
            Name (_STR, Unicode ("QUP_0_SE_7"))
            Method (_CRS, 0, Serialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x0099C000, 0x00004000)
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, , , ) { 0x00000280 }
                })
                Return (RBUF)
            }
        }
        Device (IC10)
        {
            Name (_HID, "QCOM1A10")
            Alias (\_SB.PSUB, _SUB)
            Name (_UID, 0x0A)
            Name (_DEP, Package (0x01) { \_SB.PEP0 })
            Name (_CCA, Zero)
            Name (_STR, Unicode ("QUP_1_SE_1"))
            Method (_CRS, 0, Serialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x00A84000, 0x00004000)
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, , , ) { 0x00000182 }
                })
                Return (RBUF)
            }
        }
        Device (I2C2)
        {
            Name (_HID, "QCOM1A10")
            Alias (\_SB.PSUB, _SUB)
            Name (_UID, 0x02)
            Name (_DEP, Package (0x01) { \_SB.PEP0 })
            Name (_CCA, Zero)
            Name (_STR, Unicode ("QUP_0_SE_1"))
            Method (_CRS, 0, Serialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x00984000, 0x00004000)
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, , , ) { 0x0000027A }
                })
                Return (RBUF)
            }
        }
        Device (BAT0)
        {
            Name (_HID, "Q2QC0B01")
            Name (_UID, One)
            Name (_DEP, Package (0x03) { \_SB.I2C8, \_SB.I2C2, \_SB.IC10 })
            Name (_CRS, ResourceTemplate ()
            {
                I2cSerialBusV2 (0x0036, ControllerInitiated, 400000, AddressingMode7Bit, "\\_SB.I2C8",
                    0x00, ResourceConsumer, , Exclusive, )
                I2cSerialBusV2 (0x0069, ControllerInitiated, 400000, AddressingMode7Bit, "\\_SB.I2C8",
                    0x00, ResourceConsumer, , Exclusive, )
                I2cSerialBusV2 (0x0038, ControllerInitiated, 400000, AddressingMode7Bit, "\\_SB.I2C2",
                    0x00, ResourceConsumer, , Exclusive, )
                I2cSerialBusV2 (0x0039, ControllerInitiated, 400000, AddressingMode7Bit, "\\_SB.IC10",
                    0x00, ResourceConsumer, , Exclusive, )
            })
            Method (_STA, 0, NotSerialized) { Return (0x1F) }
        }
    }
}
