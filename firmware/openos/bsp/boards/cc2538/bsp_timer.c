/**
 \brief CC2538-specific definition of the "bsp_timer" bsp module.
 
 Using sleep timer from cc2538.

 \author Xavier Vilajosana <xvilajosana@eecs.berkeley.edu>, August 2013.
 */

#include "string.h"
#include "bsp_timer.h"
#include "board.h"
#include "board_info.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_smwdthrosc.h"
#include "debug.h"
#include "interrupt.h"
#include "sleepmode.h"
#include "debugpins.h"

//=========================== defines =========================================

//=========================== variables =======================================

typedef struct {
	bsp_timer_cbt cb;
	PORT_TIMER_WIDTH last_compare_value;
	bool initiated;
	uint32_t tooclose;
	uint32_t diff;
} bsp_timer_vars_t;

bsp_timer_vars_t bsp_timer_vars;

//=========================== prototypes ======================================
void bsp_timer_isr_private();
//=========================== public ==========================================

/**
 \brief Initialize this module.

 This functions starts the timer, i.e. the counter increments, but doesn't set
 any compare registers, so no interrupt will fire.
 */
void bsp_timer_init() {

	// clear local variables
	memset(&bsp_timer_vars, 0, sizeof(bsp_timer_vars_t));

	// enable peripheral Sleeptimer
	IntRegister(INT_SMTIM, bsp_timer_isr_private);
}

/**
 \brief Register a callback.

 \param cb The function to be called when a compare event happens.
 */
void bsp_timer_set_callback(bsp_timer_cbt cb) {
	bsp_timer_vars.cb = cb;
}

/**
 \brief Reset the timer.

 This function does not stop the timer, it rather resets the value of the
 counter, and cancels a possible pending compare event.
 */
void bsp_timer_reset() {
	// reset compare

	// reset timer
    bsp_timer_vars.initiated=false;
	// record last timer compare value
	bsp_timer_vars.last_compare_value = 0;
}

/**
 \brief Schedule the callback to be called in some specified time.

 The delay is expressed relative to the last compare event. It doesn't matter
 how long it took to call this function after the last compare, the timer will
 expire precisely delayTicks after the last one.

 The only possible problem is that it took so long to call this function that
 the delay specified is shorter than the time already elapsed since the last
 compare. In that case, this function triggers the interrupt to fire right away.

 This means that the interrupt may fire a bit off, but this inaccuracy does not
 propagate to subsequent timers.

 \param delayTicks Number of ticks before the timer expired, relative to the
 last compare event.
 */
void bsp_timer_scheduleIn(PORT_TIMER_WIDTH delayTicks) {
	PORT_TIMER_WIDTH newCompareValue, current;
	PORT_TIMER_WIDTH temp_last_compare_value;

	if (!bsp_timer_vars.initiated){
		//as the timer runs forever the first time it is turned on has a weired value
		bsp_timer_vars.last_compare_value=SleepModeTimerCountGet();
		bsp_timer_vars.initiated=true;
	}

	temp_last_compare_value = bsp_timer_vars.last_compare_value;

	newCompareValue = bsp_timer_vars.last_compare_value + delayTicks + 1;
	bsp_timer_vars.last_compare_value = newCompareValue;

	current = SleepModeTimerCountGet();

	if (delayTicks < current - temp_last_compare_value) {

		// we're already too late, schedule the ISR right now manually
		// setting the interrupt flag triggers an interrupt
		bsp_timer_vars.tooclose++;
		bsp_timer_vars.diff=(current - temp_last_compare_value);
		bsp_timer_vars.last_compare_value = current;
		IntPendSet(INT_SMTIM);
	} else {
		// this is the normal case, have timer expire at newCompareValue
		SleepModeTimerCompareSet(newCompareValue);
	}
	//enable interrupt
	IntEnable(INT_SMTIM);
}

/**
 \brief Cancel a running compare.
 */
void bsp_timer_cancel_schedule() {
	// Disable the Timer0B interrupt.
	IntDisable(INT_SMTIM);
}

/**
 \brief Return the current value of the timer's counter.

 \returns The current value of the timer's counter.
 */
PORT_TIMER_WIDTH bsp_timer_get_currentValue() {
	return SleepModeTimerCountGet();
}

//=========================== private =========================================

void bsp_timer_isr_private() {
	debugpins_isr_set();
	IntPendClear(INT_SMTIM);
	bsp_timer_isr();
	debugpins_isr_clr();
}

//=========================== interrupt handlers ==============================

kick_scheduler_t bsp_timer_isr() {

	// call the callback
	bsp_timer_vars.cb();
	// kick the OS
	return KICK_SCHEDULER;
}

