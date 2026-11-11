/**
 * @file    crc8.h
 * @brief   CRC-8 used by LightingCommand and VehicleStatus (SRS COM-004).
 *
 * Parameters (project-defined, SAE-J1850 style): polynomial 0x1D, initial value 0xFF,
 * final XOR 0xFF, no input or output reflection.
 *
 * This is an application-level integrity check. It is E2E-inspired and is NOT the AUTOSAR E2E library.
 *
 * Layer: Communication. Host-testable: yes.
 */
#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>

#define CRC8_POLYNOMIAL    (0x1Du)
#define CRC8_INITIAL_VALUE (0xFFu)
#define CRC8_FINAL_XOR     (0xFFu)

/**
 * Expected CRC of the ASCII bytes "123456789" for the parameters above.
 * [B] The value 0x4B comes from the public CRC catalogue as I recall it. Confirm it with a trusted
 * independent tool before the unit test in Phase 5 relies on it.
 */
#define CRC8_CHECK_VALUE_123456789 (0x4Bu)

/**
 * @brief  Compute the CRC-8 of a byte sequence.
 * @param  data    Bytes to protect. May be NULL only when length is 0.
 * @param  length  Number of bytes.
 * @return CRC-8 value. For length 0 or NULL data the result is the initial value after final XOR.
 */
uint8_t Crc8_Compute(const uint8_t *data, uint8_t length);

#endif /* CRC8_H */
