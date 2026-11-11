# Communication (Lamp ECU)

Reserved for Lamp-specific communication glue if it turns out to be needed.
The shared communication modules (`protocol`, `crc8`, `rx_queue`, `tx_scheduler`) live in `firmware/common/`
because both ECUs use them.
