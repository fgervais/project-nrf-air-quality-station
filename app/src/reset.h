int show_reset_cause(void);
int clear_reset_cause(void);
bool is_reset_cause_watchdog(uint32_t cause);
bool is_reset_cause_button(uint32_t cause);