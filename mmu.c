#include <stdlib.h>
#include "machine.h"
#include "mmu.h"

/* The 'small' MMU is an I/O device that allows remapping a window of
a single device into a fixed region of the CPU. */
struct small_mmu
{
	struct hw_device *realdev;
	unsigned int page_size;
	U8 global_regs[8];
	U8 slot_regs[SMR_SLOTS][2];
};

void small_mmu_update_slot (struct small_mmu *mmu, unsigned int slot)
{
	unsigned int page = mmu->slot_regs[slot][SMR_PAGE - SM_GLOBAL_REGS];
	unsigned int flags = mmu->slot_regs[slot][SMR_FLAGS - SM_GLOBAL_REGS];
	bus_map (slot * SMR_PAGESIZE, mmu->realdev->devid, page, SMR_PAGESIZE,
		flags & MAP_READWRITE);
}

void small_mmu_update_current (struct small_mmu *mmu)
{
	small_mmu_update_slot (mmu, mmu->global_regs[0]);
}

void small_mmu_update_all (struct small_mmu *mmu)
{
	unsigned int slot;
	for (slot = 0; slot < SMR_SLOTS; slot++)
		small_mmu_update_slot (mmu, slot);
}

U8 small_mmu_read (struct hw_device *dev, unsigned long addr)
{
	struct small_mmu *mmu = (struct small_mmu *)dev->priv;
	if (addr < SM_GLOBAL_REGS)
		return mmu->global_regs[addr];
	else
		return mmu->slot_regs[mmu->global_regs[0]][addr - SM_GLOBAL_REGS];
}

void small_mmu_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	struct small_mmu *mmu = (struct small_mmu *)dev->priv;

	if (addr < SM_GLOBAL_REGS)
		mmu->global_regs[addr] = val;
	else
		mmu->slot_regs[mmu->global_regs[0]][addr - SM_GLOBAL_REGS] = val;

	switch (addr)
	{
		case SMR_PAGE:
		case SMR_FLAGS:
		{
			unsigned int slot = mmu->global_regs[0];
			unsigned int page = mmu->slot_regs[slot][SMR_PAGE - SM_GLOBAL_REGS];
			unsigned int flags = mmu->slot_regs[slot][SMR_FLAGS - SM_GLOBAL_REGS];
			bus_map (slot * SMR_PAGESIZE, mmu->realdev->devid, page * SMR_PAGESIZE,
				SMR_PAGESIZE, flags & MAP_READWRITE);
			break;
		}
	}
}

void small_mmu_reset (struct hw_device *dev)
{
	unsigned int page;

	for (page = 0; page < SMR_SLOTS; page++)
	{
		small_mmu_write (dev, SMR_SLOT, page);
		small_mmu_write (dev, SMR_PAGE, page);
		small_mmu_write (dev, SMR_FLAGS, MAP_READWRITE);
	}
}

struct hw_class small_mmu_class =
{
	.name = "small_mmu",
	.readonly = 0,
	.reset = small_mmu_reset,
	.read = small_mmu_read,
	.write = small_mmu_write,
};

struct hw_device* small_mmu_create (struct hw_device *realdev)
{
	struct small_mmu *mmu = malloc (sizeof (struct small_mmu));
	mmu->realdev = realdev;
	mmu->page_size = SMR_PAGESIZE;
	return device_attach (&small_mmu_class, 16, mmu); /* 16 = sizeof I/O window */
}


