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

#include "../cupkee_module_pwm.h"

#define PWM_MAX  4
#define CHN_MAX  16

typedef struct pwm_chn_t {
    uint8_t pin;
    uint8_t duty;
    uint8_t duty_new;
    uint8_t state;
} pwm_chn_t;

typedef struct pwm_t {
    uint8_t  inused;
    uint8_t  chn_num;
    uint8_t  chn_invert;
    uint8_t  chn_min_duty;

    uint8_t  step;
    uint8_t  counter;
    uint16_t interval;

    void     *timer;

    pwm_chn_t chn[0];
} pwm_t;

static void **pwm_instance = NULL;

static void pwm_reload(pwm_t *pwm)
{
    int i;
    pwm->counter = 0;
    pwm->chn_invert = 0;
    pwm->chn_min_duty = 200;

    for (i = 0; i < pwm->chn_num; i++) {
        uint8_t duty = pwm->chn[i].duty_new;

        pwm->chn[i].duty = duty;
        if (duty) {
            if (pwm->chn_min_duty > duty) {
                pwm->chn_min_duty = duty;
            }

            pwm->chn_invert++;
            pwm->chn[i].state = 1;
            cupkee_pin_set(pwm->chn[i].pin, 1);
        } else {
            pwm->chn[i].state = 0;
            cupkee_pin_set(pwm->chn[i].pin, 0);
        }
    }
}

static void pwm_invert(pwm_t *pwm)
{
    uint16_t counter;

    pwm->counter += pwm->step;
    counter = pwm->counter;

    if (!pwm->chn_invert || counter < pwm->chn_min_duty) {
        return;
    } else {
        int i;

        pwm->chn_min_duty = 200;
        for (i = 0; pwm->chn_invert > 0 && i < pwm->chn_num; i++) {
            if (pwm->chn[i].state && pwm->chn[i].duty <= counter) {
                pwm->chn_invert--;
                pwm->chn[i].state = 0;
                cupkee_pin_set(pwm->chn[i].pin, 0);
            } else {
                if (pwm->chn_min_duty > pwm->chn[i].duty) {
                    pwm->chn_min_duty = pwm->chn[i].duty;
                }
            }
        }
    }
}

static int pwm_timer_handle(void *timer, int event, intptr_t param)
{
    pwm_t *pwm = param < PWM_MAX ? pwm_instance[param] : NULL;
    int i;

    (void) timer;

    if (pwm) {
        if (event == CUPKEE_EVENT_REWIND) {
            if (pwm->counter < 199) {
                pwm_invert(pwm);
            } else {
                pwm_reload(pwm);
            }
            return 0;
        } else
        if (event == CUPKEE_EVENT_START) {
            pwm_reload(pwm);
            return 0;
        } else
        if (event == CUPKEE_EVENT_DESTROY) {
            for (i = 0; i < pwm->chn_num; i++) {
                cupkee_pin_set(pwm->chn[i].pin, 0);
            }
            cupkee_free(pwm);
            pwm_instance[param] = NULL;
        }
    }

    return 0;
}

static int pwm_request(int inst)
{
    pwm_t *pwm;
    void *timer;

    if (inst >= PWM_MAX || pwm_instance[inst]) {
        return -CUPKEE_EINVAL;
    }

    pwm = cupkee_malloc(sizeof(pwm_t));
    if (!pwm) {
        return -CUPKEE_ERESOURCE;
    }

    timer = cupkee_timer_request(pwm_timer_handle, inst);
    if (timer == NULL) {
        cupkee_free(pwm);
        return -CUPKEE_ERESOURCE;
    }

    pwm->timer = timer;
    pwm_instance[inst] = pwm;
    return 0;
}

static int pwm_release(int inst)
{
    pwm_t *pwm = inst < PWM_MAX ? pwm_instance[inst] : NULL;

    if (pwm) {
        return cupkee_release(pwm->timer);
    } else {
        return -CUPKEE_EINVAL;
    }
}

static int pwm_setup(int inst, void *entry)
{
    pwm_t *pwm = inst < PWM_MAX ? pwm_instance[inst] : NULL;
    cupkee_struct_t *conf;
    unsigned int period, interval;
    uint8_t chn_num;
    const uint8_t *chn_seq;
    int i = 0;

    if (!pwm) {
        return -CUPKEE_EINVAL;
    }

    conf = cupkee_device_config(entry);
    if (!conf) {
        return -CUPKEE_ERROR;
    }

    cupkee_struct_get_uint(conf, 0, &period);
    chn_num = cupkee_struct_get_bytes(conf, 1, &chn_seq);
    if (chn_num < 1) {
        return -CUPKEE_EINVAL;
    }

    if (period >= 2000) {
        pwm->step = 1;
        interval = period / 200;
    } else
    if (period >= 20) {
        pwm->step = 2000 / period;
        interval = 10;
    } else {
        return -CUPKEE_EINVAL;
    }

    pwm->interval = interval;
    pwm->counter = 200;

    pwm->chn_num = chn_num;
    pwm->chn_invert = 0;
    pwm->chn_min_duty = 200;

    for (i = 0; i < chn_num; i++) {
        pwm->chn[i].pin = chn_seq[i];
        pwm->chn[i].duty_new = 0;
    }

    return cupkee_timer_start(pwm->timer, pwm->interval);
}

static int pwm_reset(int inst)
{
    pwm_t *pwm = inst < PWM_MAX ? pwm_instance[inst] : NULL;

    if (pwm) {
        return cupkee_timer_stop(pwm->timer);
    }

    return -CUPKEE_EINVAL;
}

static int pwm_get(int inst, int chn, uint32_t *data)
{
    pwm_t *pwm = inst < PWM_MAX ? pwm_instance[inst] : NULL;

    if (pwm && pwm->chn_num > chn) {
        *data = pwm->chn[chn].duty;
        return 1;
    }
    return -CUPKEE_EINVAL;
}

static int pwm_set(int inst, int chn, uint32_t v)
{
    pwm_t *pwm = inst < PWM_MAX ? pwm_instance[inst] : NULL;

    if (pwm && pwm->chn_num > chn) {
        pwm->chn[chn].duty_new = v;
        return 1;
    }
    return -CUPKEE_EINVAL;
}

static const cupkee_struct_desc_t conf_desc[] = {
    {
        .name = "period",
        .type = CUPKEE_STRUCT_UINT32
    },
    {
        .name = "channel",
        .size = 16,
        .type = CUPKEE_STRUCT_OCT
    }
};

static cupkee_struct_t *pwm_conf_init(void *curr)
{
    cupkee_struct_t *conf;

    if (curr) {
        cupkee_struct_reset(curr);
        conf = curr;
    } else {
        conf = cupkee_struct_alloc(2, conf_desc);
    }

    if (conf) {
        cupkee_struct_set_uint(conf, 0, 20000); // default period 20ms
    }

    return conf;
}

static const cupkee_driver_t pwm_driver = {
    .request = pwm_request,
    .release = pwm_release,
    .reset   = pwm_reset,
    .setup   = pwm_setup,

    .get    = pwm_get,
    .set    = pwm_set,
};

static const cupkee_device_desc_t hw_device_pwm = {
    .name = "pwm",
    .inst_max = PWM_MAX,
    .conf_init = pwm_conf_init,
    .driver = &pwm_driver
};

int cupkee_module_setup_pwm(void)
{
    int i;

    for (i = 0; i < PWM_MAX; i++) {
        pwm_instance[i] = NULL;
    }

    return cupkee_device_register(&hw_device_pwm);
}

