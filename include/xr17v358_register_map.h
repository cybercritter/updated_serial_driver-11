/**
 * @file xr17v358_register_map.h
 * @brief Register map and port-layout definitions for the XR17V358 device.
 *
 * @details
 * This file centralizes the XR17V358 address map used by the driver and tests.
 * It defines the per-port MMIO spacing, symbolic register offsets, MCR bit
 * masks, and a compact descriptive register table that can be displayed in
 * generated documentation.  Keeping the register definitions here avoids
 * scattering hard-coded offsets throughout the codebase and makes it easier to
 * audit address calculations.
 */
#ifndef XR17V358_REGISTER_MAP_H
#define XR17V358_REGISTER_MAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of UART ports exposed by the XR17V358 device. */
#define XR17V358_PORT_COUNT 8U
/** Distance in bytes between adjacent per-port MMIO windows. */
#define XR17V358_PORT_STRIDE 0x0400U
/** Size of one per-port MMIO window. */
#define XR17V358_PORT_WINDOW_SIZE XR17V358_PORT_STRIDE

/**
 * @brief Descriptive register-map entry used for documentation and lookup.
 */
typedef struct xr17v358_register_map_entry {
  /** Human-readable register name. */
  const char *name;
  /** Register offset relative to a port base. */
  uint32_t offset;
  /** Short functional description. */
  const char *description;
} xr17v358_register_map_entry;

/**
 * @brief Symbolic per-port XR17V358 register offsets.
 */
enum xr17v358_register_offset {
  XR17V358_REG_RHR = 0x00U,
  XR17V358_REG_THR = 0x00U,
  XR17V358_REG_DLL = 0x00U,
  XR17V358_REG_IER = 0x01U,
  XR17V358_REG_DLM = 0x01U,
  XR17V358_REG_ISR = 0x02U,
  XR17V358_REG_FCR = 0x02U,
  XR17V358_REG_EFR = 0x02U,
  XR17V358_REG_LCR = 0x03U,
  XR17V358_REG_MCR = 0x04U,
  XR17V358_REG_XON2 = 0x04U,
  XR17V358_REG_LSR = 0x05U,
  XR17V358_REG_XON1 = 0x05U,
  XR17V358_REG_MSR = 0x06U,
  XR17V358_REG_XOFF2 = 0x06U,
  XR17V358_REG_SPR = 0x07U,
  XR17V358_REG_XOFF1 = 0x07U,
  XR17V358_REG_TXLVL = 0x08U,
  XR17V358_REG_RXLVL = 0x09U,
  XR17V358_REG_IODIR = 0x0AU,
  XR17V358_REG_IOSTATE = 0x0BU,
  XR17V358_REG_IOINTENA = 0x0CU,
  XR17V358_REG_IOCONTROL = 0x0EU,
  XR17V358_REG_EFCR = 0x0FU,
  XR17V358_REG_XON1_FLOW = 0x10U,
  XR17V358_REG_XON2_FLOW = 0x11U,
  XR17V358_REG_XOFF1_FLOW = 0x12U,
  XR17V358_REG_XOFF2_FLOW = 0x13U,
  XR17V358_REG_FIFO = 0x0100U,
};

/**
 * @brief Bit masks for the XR17V358 modem control register.
 */
enum xr17v358_mcr_bits {
  XR17V358_MCR_DTR = 0x01U,
  XR17V358_MCR_RTS = 0x02U,
  XR17V358_MCR_OUT1 = 0x04U,
  XR17V358_MCR_OUT2 = 0x08U,
  XR17V358_MCR_LOOPBACK = 0x10U,
};

/** Static MMIO offsets for each UART port. */
static const uint32_t xr17v358_port_offsets[XR17V358_PORT_COUNT] = {
    0x0000U, 0x0400U, 0x0800U, 0x0C00U,
    0x1000U, 0x1400U, 0x1800U, 0x1C00U,
};

/** Compact descriptive table of the UART register map. */
static const xr17v358_register_map_entry xr17v358_uart_register_map[] = {
    {"RHR/THR/DLL", XR17V358_REG_RHR,
     "Receive holding, transmit holding, divisor latch low"},
    {"IER/DLM", XR17V358_REG_IER,
     "Interrupt enable, divisor latch high"},
    {"ISR/FCR/EFR", XR17V358_REG_ISR,
     "Interrupt status, FIFO control, enhanced feature"},
    {"LCR", XR17V358_REG_LCR, "Line control"},
    {"MCR", XR17V358_REG_MCR, "Modem control"},
    {"LSR", XR17V358_REG_LSR, "Line status"},
    {"MSR", XR17V358_REG_MSR, "Modem status"},
    {"SPR", XR17V358_REG_SPR, "Scratchpad"},
    {"TXLVL", XR17V358_REG_TXLVL, "Transmit FIFO level"},
    {"RXLVL", XR17V358_REG_RXLVL, "Receive FIFO level"},
    {"IODIR", XR17V358_REG_IODIR, "GPIO direction"},
    {"IOSTATE", XR17V358_REG_IOSTATE, "GPIO state"},
    {"IOINTENA", XR17V358_REG_IOINTENA, "GPIO interrupt enable"},
    {"IOCONTROL", XR17V358_REG_IOCONTROL, "GPIO control"},
    {"EFCR", XR17V358_REG_EFCR, "Extra features control"},
    {"XON1", XR17V358_REG_XON1_FLOW, "Software flow-control XON1"},
    {"XON2", XR17V358_REG_XON2_FLOW, "Software flow-control XON2"},
    {"XOFF1", XR17V358_REG_XOFF1_FLOW, "Software flow-control XOFF1"},
    {"XOFF2", XR17V358_REG_XOFF2_FLOW, "Software flow-control XOFF2"},
    {"FIFO", XR17V358_REG_FIFO, "Memory-mapped FIFO data window"},
};

/** Number of entries in @ref xr17v358_uart_register_map. */
#define XR17V358_UART_REGISTER_MAP_SIZE \
  (sizeof(xr17v358_uart_register_map) / sizeof(xr17v358_uart_register_map[0]))

/**
 * @brief Return the MMIO offset for a selected UART port.
 * @param port_index Zero-based UART port number.
 * @return Port offset relative to the device base.
 */
static inline uint32_t xr17v358_get_port_offset_from_map(size_t port_index) {
  return xr17v358_port_offsets[port_index];
}

/**
 * @brief Compute an absolute register address from device base, port, and
 * register offset.
 * @param device_base Base address of the XR17V358 device.
 * @param port_index Zero-based UART port number.
 * @param register_offset Register offset relative to the port base.
 * @return Absolute address for the requested register.
 */
static inline uint32_t xr17v358_get_register_address(uint32_t device_base,
                                                     size_t port_index,
                                                     uint32_t register_offset) {
  return device_base + xr17v358_get_port_offset_from_map(port_index) +
         register_offset;
}

#ifdef __cplusplus
}
#endif

#endif
