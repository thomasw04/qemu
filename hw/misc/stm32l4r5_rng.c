
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/guest-random.h"
#include "qemu/units.h"
#include "qemu/module.h"
#include "hw/misc/stm32l4r5_rng.h"
#include "hw/qdev-properties.h"
#include "trace.h"

enum {
    RNG_CR = 0,
    RNG_SR = 1,
    RNG_DR = 2,
};

#define REG_TO_OFFSET(reg) ((reg) * 4)
#define OFFSET_TO_REG(offset) ((offset) / 4)


static uint64_t stm32l4r5_rng_read(void *opaque, hwaddr offset, unsigned size)
{
    STM32L4R5RNGState *s = opaque;
    uint64_t value = 0;

    if (offset >= STM32L4R5_RNG_REGS_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: out-of-bounds offset 0x%04x\n",
                      __func__, (uint32_t)offset);
        return 0;
    }

    switch (OFFSET_TO_REG(offset)) 
    {
    case RNG_CR:
        value = s->regs[RNG_CR];
        break;
    case RNG_SR:
        value = s->regs[RNG_SR];
        break;
    case RNG_DR:
        if (s->regs[RNG_SR] & 0x1) {
            qemu_guest_getrandom(&value, sizeof(value), NULL);
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: No valid data available in DR register.\n", __func__);
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: read from invalid offset 0x%" HWADDR_PRIx "\n",
                      DEVICE(s)->canonical_path, offset);
        break;
    }

    return value;
}

static void stm32l4r5_rng_write(void *opaque, hwaddr offset, uint64_t value,
                               unsigned size)
{
    STM32L4R5RNGState *s = opaque;

    switch (offset) {
    case RNG_CR:
        s->regs[RNG_CR] = value;

        // Set DRDY bit if the RNG is enabled.
        if (value & 0x4) {
            s->regs[RNG_SR] |= 0x1;
        } else {
            s->regs[RNG_SR] &= ~0x1;
        }
        break;
    case RNG_SR:
        // Warn when bit 2 1 or 0 is set.
        if (value & 0x1 || value & 0x2 || value & 0x4) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Guest tries to write SR with read only bits set, this might be a bug. @ 0x%" HWADDR_PRIx "\n", DEVICE(s)->canonical_path, offset);
        } 

        // Clear bit 6 if its 0.
        if (!(value & 0x40)) {
            s->regs[RNG_SR] &= ~0x40;
        }

        // Clear bit 5 if its 0.
        if (!(value & 0x20)) {
            s->regs[RNG_SR] &= ~0x20;
        }
        break;
    case RNG_DR:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to read-only register @ 0x%" HWADDR_PRIx "\n",
                      DEVICE(s)->canonical_path, offset);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to invalid offset 0x%" HWADDR_PRIx "\n",
                      DEVICE(s)->canonical_path, offset);
        break;
    }
}


static const MemoryRegionOps stm32l4r5_rng_ops = {
    .read = stm32l4r5_rng_read,
    .write = stm32l4r5_rng_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};


static void stm32l4r5_rng_realize(DeviceState *dev, Error **errp)
{
    STM32L4R5RNGState *s = STM32L4R5_RNG(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &stm32l4r5_rng_ops, s,
                          TYPE_STM32L4R5_RNG, STM32L4R5_RNG_REGS_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void stm32l4r5_rng_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dklass = DEVICE_CLASS(klass);

    dklass->desc = "STM32L4R5 True Random Number Generator";
    dklass->realize = stm32l4r5_rng_realize;
}


static TypeInfo stm32l4r5_rng_info = {
    .name = TYPE_STM32L4R5_RNG,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32L4R5RNGState),
    .class_init = stm32l4r5_rng_class_init,
};

static void stm32l4r5_rng_register_types(void)
{
    type_register_static(&stm32l4r5_rng_info);
}

type_init(stm32l4r5_rng_register_types)