/*
 * STM32 Nucleo-144 development board with STM32L4R5ZI MCU (Cortex-M4)
 * 
 * This is based on the code of:
 * 
 * B-L475E-IOT01A Discovery Kit machine
 * (B-L475E-IOT01A IoT Node)
 *
 * Copyright (c) 2023-2024 Arnaud Minier <arnaud.minier@telecom-paris.fr>
 * Copyright (c) 2023-2024 Inès Varhol <ines.varhol@telecom-paris.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * This work is heavily inspired by the netduinoplus2 by Alistair Francis.
 * Original code is licensed under the MIT License:
 *
 * Copyright (c) 2014 Alistair Francis <alistair@alistair23.me>
 */

/*
 * The reference used is the STMicroElectronics datasheet
 * https://www.st.com/resource/en/datasheet/stm32l4r5vi.pdf
*/

/* Changes made by Thomas Wachter (thomasw04) */

/* Reference used is: https://www.st.com/resource/en/datasheet/stm32l4r5vi.pdf */


#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "qemu/error-report.h"
#include "hw/arm/boot.h"
#include "hw/core/split-irq.h"
#include "hw/arm/stm32l4r5_soc.h"
#include "hw/gpio/stm32l4x5_gpio.h"
#include "hw/display/dm163.h"

/* STM32L4R5 implementation is inspired from netduinoplus2 and arduino */

/*
 * There are actually 14 input pins in the DM163 device.
 * Here the DM163 input pin EN isn't connected to the STM32L4x5
 * GPIOs as the IM120417002 colors shield doesn't actually use
 * this pin to drive the RGB matrix.
 */
#define NUM_DM163_INPUTS 13

static const unsigned dm163_input[NUM_DM163_INPUTS] = {
    1 * GPIO_NUM_PINS + 2,  /* ROW0  PB2       */
    0 * GPIO_NUM_PINS + 15, /* ROW1  PA15      */
    0 * GPIO_NUM_PINS + 2,  /* ROW2  PA2       */
    0 * GPIO_NUM_PINS + 7,  /* ROW3  PA7       */
    0 * GPIO_NUM_PINS + 6,  /* ROW4  PA6       */
    0 * GPIO_NUM_PINS + 5,  /* ROW5  PA5       */
    1 * GPIO_NUM_PINS + 0,  /* ROW6  PB0       */
    0 * GPIO_NUM_PINS + 3,  /* ROW7  PA3       */
    0 * GPIO_NUM_PINS + 4,  /* SIN (SDA) PA4   */
    1 * GPIO_NUM_PINS + 1,  /* DCK (SCK) PB1   */
    2 * GPIO_NUM_PINS + 3,  /* RST_B (RST) PC3 */
    2 * GPIO_NUM_PINS + 4,  /* LAT_B (LAT) PC4 */
    2 * GPIO_NUM_PINS + 5,  /* SELBK (SB)  PC5 */
};

#define TYPE_STM32NUCLEO_L4R5ZI MACHINE_TYPE_NAME("stm32nucleo-l4r5zi")
OBJECT_DECLARE_SIMPLE_TYPE(STM32NUCLEO_L4R5ZIMachineState, STM32NUCLEO_L4R5ZI)

typedef struct STM32NUCLEO_L4R5ZIMachineState {
    MachineState parent_obj;

    Stm32l4r5SocState soc;
    SplitIRQ gpio_splitters[NUM_DM163_INPUTS];
    DM163State dm163;
} STM32NUCLEO_L4R5ZIMachineState;

static void stm32nucleo_l4r5zi_init(MachineState *machine)
{
    STM32NUCLEO_L4R5ZIMachineState *s = STM32NUCLEO_L4R5ZI(machine);
    const Stm32l4r5SocClass *sc;
    DeviceState *dev, *gpio_out_splitter;
    unsigned gpio, pin;

    object_initialize_child(OBJECT(machine), "soc", &s->soc,
                            TYPE_STM32L4R5XI_SOC);
    sysbus_realize(SYS_BUS_DEVICE(&s->soc), &error_fatal);

    sc = STM32L4R5_SOC_GET_CLASS(&s->soc);
    armv7m_load_kernel(ARM_CPU(first_cpu), machine->kernel_filename, 0,
                       sc->flash_size);

    if (object_class_by_name(TYPE_DM163)) {
        object_initialize_child(OBJECT(machine), "dm163",
                                &s->dm163, TYPE_DM163);
        dev = DEVICE(&s->dm163);
        qdev_realize(dev, NULL, &error_abort);

        for (unsigned i = 0; i < NUM_DM163_INPUTS; i++) {
            object_initialize_child(OBJECT(machine), "gpio-out-splitters[*]",
                                    &s->gpio_splitters[i], TYPE_SPLIT_IRQ);
            gpio_out_splitter = DEVICE(&s->gpio_splitters[i]);
            qdev_prop_set_uint32(gpio_out_splitter, "num-lines", 2);
            qdev_realize(gpio_out_splitter, NULL, &error_fatal);

            qdev_connect_gpio_out(gpio_out_splitter, 0,
                qdev_get_gpio_in(DEVICE(&s->soc), dm163_input[i]));
            qdev_connect_gpio_out(gpio_out_splitter, 1,
                qdev_get_gpio_in(dev, i));
            gpio = dm163_input[i] / GPIO_NUM_PINS;
            pin = dm163_input[i] % GPIO_NUM_PINS;
            qdev_connect_gpio_out(DEVICE(&s->soc.gpio[gpio]), pin,
                qdev_get_gpio_in(DEVICE(gpio_out_splitter), 0));
        }
    }
}

static void stm32nucleo_l4r5zi_machine_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    static const char *machine_valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m4"),
        NULL
    };
    mc->desc = "STM32 Nucleo-144 development board with STM32L4R5ZI MCU (Cortex-M4)";
    mc->init = stm32nucleo_l4r5zi_init;
    mc->valid_cpu_types = machine_valid_cpu_types;

    /* SRAM pre-allocated as part of the SoC instantiation */
    mc->default_ram_size = 2 * MiB;
}

static const TypeInfo stm32nucleo_l4r5zi_machine_type[] = {
    {
        .name           = TYPE_STM32NUCLEO_L4R5ZI,
        .parent         = TYPE_MACHINE,
        .instance_size  = sizeof(STM32NUCLEO_L4R5ZIMachineState),
        .class_init     = stm32nucleo_l4r5zi_machine_init,
    }
};

DEFINE_TYPES(stm32nucleo_l4r5zi_machine_type)
