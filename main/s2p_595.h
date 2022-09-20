#ifndef S2P_595
#define S2P_595

#include <stdint.h>

struct s2p_595
{
    uint8_t pin_serial_in;     // serial in
    uint8_t pin_shift_clock;   // shift clock (rising)
    uint8_t pin_latch_clock;   // latch clock (rising)
    uint8_t pin_reset;         // reset (active low)
    uint8_t pin_output_enable; // output enable (active low)
};

void s2p_595_setup(struct s2p_595 *s2p);

void s2p_595_set_input(struct s2p_595 *s2p, int input);
void s2p_595_shift_edge(struct s2p_595 *s2p);
void s2p_595_latch_edge(struct s2p_595 *s2p);

void s2p_595_reset(struct s2p_595 *s2p);
void s2p_595_disable_output(struct s2p_595 *s2p);
void s2p_595_enable_output(struct s2p_595 *s2p);

#endif /* S2P_595 */
