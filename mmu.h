#ifndef MMU_H
#define MMU_H

#define SMR_PAGESIZE 4096

#define SMR_SLOTS 16

/* Small MMU register map */
#define SMR_SLOT   0  /* Which slot is described by registers 8-9 */
#define SMR_BASEL  1  /* The base page for lower 8 slots */
#define SMR_BASEH  2  /* The base page for upper 8 slots */
#define SMR_FAULTA 3  /* The faulting address */
#define SMR_FAULTT 5  /* The fault type */
#define SM_GLOBAL_REGS 6
#define SMR_PAGE   6  /* Which 4KB page is mapped to the current slot */
#define SMR_FLAGS  7  /* What are the page flags for this slot */

/* The 'small' MMU is an I/O device that allows remapping a window of
a single device into a fixed region of the CPU. */
struct small_mmu;
void small_mmu_update_slot (struct small_mmu *mmu, unsigned int slot);
void small_mmu_update_current (struct small_mmu *mmu);
void small_mmu_update_all (struct small_mmu *mmu);
U8 small_mmu_read (struct hw_device *dev, unsigned long addr);
void small_mmu_write (struct hw_device *dev, unsigned long addr, U8 val);
void small_mmu_reset (struct hw_device *dev);
struct hw_device* small_mmu_create (struct hw_device *realdev);

#endif
