#include "ofp.h"
#include "s2p_595.h"

#include <driver/gpio.h>

void s2p_595_setup(struct s2p_595 *s2p)
{
    assert(s2p != NULL);

    ofp_pin_setup_output_no_pull(s2p->pin_serial_in);
    ofp_pin_setup_output_no_pull(s2p->pin_shift_clock);
    ofp_pin_setup_output_no_pull(s2p->pin_latch_clock);
    ofp_pin_setup_output_no_pull(s2p->pin_reset);
    ofp_pin_setup_output_no_pull(s2p->pin_output_enable);

    // disable output while setting up
    s2p_595_disable_output(s2p);

    // at the same time
    gpio_set_level(s2p->pin_serial_in, 0);
    gpio_set_level(s2p->pin_shift_clock, 0);
    gpio_set_level(s2p->pin_latch_clock, 0);

    // reset shift registers
    s2p_595_reset(s2p);

    // snapshot reset'ed registers
    s2p_595_latch_edge(s2p);

    // enable output
    s2p_595_enable_output(s2p);
}

void s2p_595_set_input(struct s2p_595 *s2p, int input)
{
    ofp_pin_set_value_wait_1us(s2p->pin_serial_in, input);
}

void s2p_595_shift_edge(struct s2p_595 *s2p)
{
    ofp_pin_set_value_wait_1us(s2p->pin_shift_clock, 0);
    ofp_pin_set_value_wait_1us(s2p->pin_shift_clock, 1); // trigger on rising edge
    ofp_pin_set_value_wait_1us(s2p->pin_shift_clock, 0);
}

void s2p_595_latch_edge(struct s2p_595 *s2p)
{
    ofp_pin_set_value_wait_1us(s2p->pin_latch_clock, 0);
    ofp_pin_set_value_wait_1us(s2p->pin_latch_clock, 1); // trigger on rising edge
    ofp_pin_set_value_wait_1us(s2p->pin_latch_clock, 0);
}

void s2p_595_reset(struct s2p_595 *s2p)
{
    ofp_pin_set_value_wait_1us(s2p->pin_reset, 0); // active low
    ofp_pin_set_value_wait_1us(s2p->pin_reset, 1);
}

void s2p_595_disable_output(struct s2p_595 *s2p)
{
    ofp_pin_set_value_wait_1us(s2p->pin_output_enable, 1);
}

void s2p_595_enable_output(struct s2p_595 *s2p)
{
    ofp_pin_set_value_wait_1us(s2p->pin_output_enable, 0);
}
