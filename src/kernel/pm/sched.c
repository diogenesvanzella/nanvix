/*
 * Copyright(C) 2011-2016 Pedro H. Penna   <pedrohenriquepenna@gmail.com>
 *              2015-2016 Davidson Francis <davidsondfgl@hotmail.com>
 *
 * This file is part of Nanvix.
 *
 * Nanvix is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nanvix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nanvix. If not, see <http://www.gnu.org/licenses/>.
 */

#include <nanvix/clock.h>
#include <nanvix/const.h>
#include <nanvix/hal.h>
#include <nanvix/pm.h>
#include <signal.h>
#include <nanvix/klib.h>

int rand(void) {
	unsigned long int seed = CURRENT_TIME * 1103515245 + 12345;
	return (unsigned int) (seed/65536) % 32768;
}

int next_process(int total_tickets){
	return (rand()*total_tickets/32768)+1;
}

/**
 * @brief Schedules a process to execution.
 * 
 * @param proc Process to be scheduled.
 */
PUBLIC void sched(struct process *proc)
{
	proc->state = PROC_READY;
	proc->counter = 0;
}

/**
 * @brief Stops the current running process.
 */
PUBLIC void stop(void)
{
	curr_proc->state = PROC_STOPPED;
	sndsig(curr_proc->father, SIGCHLD);
	yield();
}

/**
 * @brief Resumes a process.
 * 
 * @param proc Process to be resumed.
 * 
 * @note The process must stopped to be resumed.
 */
PUBLIC void resume(struct process *proc)
{	
	/* Resume only if process has stopped. */
	if (proc->state == PROC_STOPPED)
		sched(proc);
}

/**
 * Add compensation tickets for processes that are loosing processor 
 * and do not use their entire quantum
 */
PUBLIC void add_compensation() {
	if (curr_proc->counter > 0 && curr_proc->counter != PROC_QUANTUM) {
		float used_quantum = PROC_QUANTUM - curr_proc->counter;
		float fraction = used_quantum / PROC_QUANTUM;
		float compensation = curr_proc->tickets / fraction;
		int final = (unsigned int) compensation - curr_proc->tickets;
		curr_proc->compensation = final;
	}
}

/**
 * @brief Yields the processor.
 */
PUBLIC void yield(void)
{
	struct process *p;    	/* Working process.     */
	struct process *next; 	/* Next process to run. */
	int total_tickets = 0; 	/* Number of tickets of all process. */

	/* Re-schedule process for execution. */
	if (curr_proc->state == PROC_RUNNING) {
		/* Current process must be compensate. */
		add_compensation();
		sched(curr_proc);
	}

	/* Remember this process. */
	last_proc = curr_proc;

	/* Check alarm. */
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {

		/* count tickets of all ready process. */
		if (p->state == PROC_READY) {
			total_tickets += (p->tickets + p->compensation);
		}

		/* Skip invalid processes. */
		if (!IS_VALID(p))
			continue;
		
		/* Alarm has expired. */
		if ((p->alarm) && (p->alarm < ticks))
			p->alarm = 0, sndsig(p, SIGALRM);
	}

	/* Choose a process to run next. */
	next = IDLE;
	/* get a random number between [1...tickets_sum]. */
	int sorted_ticket = next_process(total_tickets);
	int tickets_sum = 0; /* Sum of tickets. */
	/**
	 * Choose the process that contains the winner ticket
	 */
	for (p = FIRST_PROC; p <= LAST_PROC; p++) 
	{
		/* Skip non-ready process. */
		if (p->state != PROC_READY)
			continue;

		tickets_sum += (p->tickets + p->compensation);
		if (tickets_sum > sorted_ticket) {
			next = p;
			break;
		}
	}

	/* Switch to next process. */
	next->priority = PRIO_USER;
	next->state = PROC_RUNNING;
	next->counter = PROC_QUANTUM;
	next->tickets = (next->priority*(-1) + NORMALIZATION_VALUE - next->nice);
	next->compensation = 0;
	switch_to(next);
}
