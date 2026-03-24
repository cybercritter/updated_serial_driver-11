#include "hw_abstraction.h"

#include <stdint.h>

#include "xr17v358.h"

static abaco_hw_error to_abaco_hw_error(xr17v358_error_t error) {
  return (abaco_hw_error)error;
}

static xr17v358_port_config_t to_xr17v358_port_config(
    const abaco_hw_port_config *config) {
  xr17v358_port_config_t mapped;

  mapped.baud_rate = config->baud_rate;
  mapped.stop_bits = (xr17v358_stop_bits_t)config->stop_bits;
  mapped.parity = (xr17v358_parity_t)config->parity;
  return mapped;
}

abaco_hw_error abaco_hw_initialize_port(size_t port_index,
                                        const abaco_hw_port_config *config) {
  xr17v358_port_config_t mapped;

  if (config == NULL) {
    return ABACO_HW_ERROR_INVALID_ARGUMENT;
  }

  mapped = to_xr17v358_port_config(config);
  return to_abaco_hw_error(xr17v358_initialize_port(port_index, &mapped));
}

abaco_hw_error abaco_hw_read(size_t port_index, uint8_t *data, size_t length,
                             size_t *bytes_read) {
  return to_abaco_hw_error(xr17v358_read(port_index, data, length, bytes_read));
}

abaco_hw_error abaco_hw_write(size_t port_index, const uint8_t *data,
                              size_t length, size_t *bytes_written) {
  return to_abaco_hw_error(
      xr17v358_write(port_index, data, length, bytes_written));
}

abaco_hw_error abaco_hw_poll_port(size_t port_index) {
  return to_abaco_hw_error(xr17v358_poll_port(port_index));
}

abaco_hw_error abaco_hw_set_discrete(size_t port_index, int asserted) {
  return to_abaco_hw_error(xr17v358_set_discrete(ABACO_HW_DEVICE_BASE_ADDRESS,
                                                 port_index, asserted));
}

abaco_hw_error abaco_hw_set_loopback(size_t port_index, int enabled) {
  return to_abaco_hw_error(
      xr17v358_set_loopback(ABACO_HW_DEVICE_BASE_ADDRESS, port_index, enabled));
}
