/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   timing.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hkanamit <hkanamit@student.42tokyo.jp>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/09 15:20:00 by hkanamit          #+#    #+#             */
/*   Updated: 2026/09/09 15:40:00 by hkanamit         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"
#include <unistd.h>

void	precise_sleep(t_shared *shared, long ms)
{
	struct timeval	start;
	struct timeval	now;
	long			target;
	long			elapsed;

	if (ms <= 0)
		return ;
	target = ms * 1000L;
	gettimeofday(&start, NULL);
	while (1)
	{
		gettimeofday(&now, NULL);
		elapsed = (now.tv_sec - start.tv_sec) * 1000000L
			+ (now.tv_usec - start.tv_usec);
		if (elapsed >= target || is_stopped(shared))
			return ;
		if (target - elapsed > 1000)
			usleep(200);
		else
			usleep(50);
	}
}
