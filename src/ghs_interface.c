#include <ghslib.h>

#ifdef GHSMULTI
#include <stddef.h>
#include <stdint.h>

#include "xr17v358.h"

#define OK 0

// /* Placeholder object type */
// typedef void *MemoryRegion;

/* These names are based on the earlier discussion.
 * Some projects expose slightly different signatures.
 */
extern int GetMemoryRegion(const char *name, MemoryRegion *mr);
extern int GetMemoryRegionExtendedAddresses(MemoryRegion mr,
                                            uintptr_t *first_addr,
                                            uintptr_t *last_addr);

extern int GetMappedPhysicalMemoryRegion(MemoryRegion virtual_mr,
                                         MemoryRegion *physical_mr);

/* Optional helper */
static int mmio_set_bits32(uintptr_t reg_addr, uint32_t mask) {
  volatile uint32_t *reg = (volatile uint32_t *)reg_addr;
  uint32_t v = *reg;
  *reg = v | mask;
  return OK;
}

int uart0_enable_tx(void) {
  int status;
  MemoryRegion virtual_mr = 0;
  MemoryRegion physical_mr = 0;

  uintptr_t vfirst = 0, vlast = 0;
  uintptr_t pfirst = 0, plast = 0;

  uintptr_t reg_addr;
  size_t vsize;

  /* 1) Resolve the virtual MMIO region by name */
  status = GetMemoryRegion("uart0_regs", &virtual_mr);
  if (status != OK) {
    return status;
  }

  /* 2) Get the mapped virtual address range */
  status = GetMemoryRegionExtendedAddresses(virtual_mr, &vfirst, &vlast);
  if (status != OK) {
    return status;
  }

  /* 3) Optional: also resolve the underlying physical MMIO window */
  status = GetMappedPhysicalMemoryRegion(virtual_mr, &physical_mr);
  if (status == OK) {
    (void)GetMemoryRegionExtendedAddresses(physical_mr, &pfirst, &plast);
    /* pfirst/plast now describe the physical device window */
  }

  /* 4) Bounds-check the register offset */
  if (vlast < vfirst) {
    return -1;
  }

  vsize = (size_t)(vlast - vfirst + 1u);

  if ((UART_CR_OFFSET + sizeof(uint32_t)) > vsize) {
    return -2;
  }

  /* 5) Compute the virtual register address */
  reg_addr = vfirst + UART_CR_OFFSET;

  /* 6) Set the TX enable bit in the control register */
  return mmio_set_bits32(reg_addr, UART_CR_TX_ENABLE_BIT);
}
#endif /* GHSMULTI */
