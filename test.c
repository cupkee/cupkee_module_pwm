/* GPLv2 License
 *
 * Copyright (C) 2017-2018 Lixing Ding <ding.lixing@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 **/

#include <cupkee.h>

#include "cupkee_module_pwm.h"

static const native_t native_entries[] = {
    /* Panda natives */

    /* Cupkee natives */
    {"print",           native_print},
    {"pinEnable",       native_pin_enable},
    {"pin",             native_pin},

    {"setTimeout",      native_set_timeout},
    {"setInterval",     native_set_interval},
    {"clearTimeout",    native_clear_timeout},
    {"clearInterval",   native_clear_interval},
};

static void board_setup(void)
{
    cupkee_module_setup_pwm();
}

static int board_native_number(void)
{
    return sizeof(native_entries) / sizeof(native_t);
}

static const native_t *board_native_entries(void)
{
    return native_entries;
}

int main(void)
{
    void *stream;

    /**********************************************************
     * Cupkee system initial
     *********************************************************/
    cupkee_init(NULL);

    stream = cupkee_device_request("uart", 0);
    if (cupkee_device_enable(stream)) {
        hw_halt();
    }
    cupkee_sdmp_init(stream);

    cupkee_shell_init(board_native_number(), board_native_entries());

    /**********************************************************
     * user setup code
     *********************************************************/
    board_setup();

    /**********************************************************
     * Let's Go!
     *********************************************************/
    cupkee_shell_loop(NULL);

    /**********************************************************
     * Let's Go!
     *********************************************************/
    return 0;
}

