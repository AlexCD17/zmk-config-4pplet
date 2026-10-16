/*
 * Taps a keycode (default: caps lock) on every state change of the bound
 * key position - both press and release. Designed for latching switches
 * (Alps SCCM caps lock), where each physical press produces exactly one
 * matrix transition that persists until the next press.
 */

#define DT_DRV_COMPAT zmk_behavior_caps_latch

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Optional: delayed release, see note below. */
static struct k_work_delayable release_work;
static uint32_t pending_keycode;
static bool use_delayed_release = false;

static void do_release(struct k_work *work)
{
	(void)work;
	raise_zmk_keycode_state_changed_from_encoded(pending_keycode, false,
	                                              (uint32_t)k_uptime_get());
}

static void tap_keycode(uint32_t keycode, uint32_t timestamp)
{
	raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);

	if (use_delayed_release) {
		pending_keycode = keycode;
		k_work_schedule_delayable(&release_work, K_MSEC(10));
	} else {
		raise_zmk_keycode_state_changed_from_encoded(keycode, false, timestamp);
	}
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
				     struct zmk_behavior_binding_event event)
{
	LOG_DBG("caps_latch position %d keycode 0x%02X pressed", event.position, binding->param1);
	tap_keycode(binding->param1, event.timestamp);
	return 0;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
				      struct zmk_behavior_binding_event event)
{
	LOG_DBG("caps_latch position %d keycode 0x%02X released", event.position, binding->param1);
	tap_keycode(binding->param1, event.timestamp);
	return 0;
}

static const struct behavior_driver_api behavior_caps_latch_driver_api = {
	.binding_pressed = on_keymap_binding_pressed,
	.binding_released = on_keymap_binding_released,
};

static int caps_latch_init(const struct device *dev)
{
	(void)dev;
	k_work_init_delayable(&release_work, do_release);
	return 0;
}

#define CAPS_LATCH_INST(n) \
	BEHAVIOR_DT_INST_DEFINE(n, caps_latch_init, NULL, NULL, NULL, POST_KERNEL, \
				CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, \
				&behavior_caps_latch_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CAPS_LATCH_INST)
