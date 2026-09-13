/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   coder_state.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hkanamit <hkanamit@student.42tokyo.jp>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/09 16:50:00 by hkanamit          #+#    #+#             */
/*   Updated: 2026/09/11 12:00:00 by hkanamit         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"

void	set_blocked_on(t_coder *coder, t_dongle *d)
{
	pthread_mutex_lock(&coder->state_lock);
	coder->blocked_on = d;
	pthread_mutex_unlock(&coder->state_lock);
}

static long	pass_over_cost_us(t_shared *shared)
{
	return ((shared->t_to_compile + shared->dongle_cooldown) * 1000L);
}

int	can_be_passed_over(t_coder *head, t_dongle *d)
{
	long	deadline_us;
	long	remaining_us;
	long	cost_us;
	int		blocked_elsewhere;

	cost_us = pass_over_cost_us(head->shared);
	pthread_mutex_lock(&head->state_lock);
	blocked_elsewhere = (head->blocked_on != NULL && head->blocked_on != d);
	deadline_us = head->last_compile_start_us
		+ head->shared->t_to_burnout * 1000L;
	pthread_mutex_unlock(&head->state_lock);
	remaining_us = deadline_us - elapsed_us(head->shared);
	return (blocked_elsewhere && remaining_us > cost_us);
}

void	mark_waiters_unblocked(t_dongle *d)
{
	int		i;
	t_coder	*waiter;

	i = 0;
	while (i < d->waiters.size)
	{
		waiter = &d->shared->coders[d->waiters.entries[i].coder_id - 1];
		pthread_mutex_lock(&waiter->state_lock);
		if (waiter->blocked_on == d)
			waiter->blocked_on = NULL;
		pthread_mutex_unlock(&waiter->state_lock);
		i++;
	}
}
