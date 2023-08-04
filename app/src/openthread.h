#ifndef OPENTHREAD_H_
#define OPENTHREAD_H_

void openthread_enable_ready_flag();
bool openthread_is_ready();
void openthread_set_csl_period_ms(int period_ms);
void openthread_set_low_latency();
void openthread_set_normal_latency();

#endif /* OPENTHREAD_H_ */