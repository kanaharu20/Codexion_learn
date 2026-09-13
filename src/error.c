/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   error.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hkanamit <hkanamit@student.42tokyo.jp>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/11 16:30:00 by hkanamit          #+#    #+#             */
/*   Updated: 2026/09/11 16:30:00 by hkanamit         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"
#include <stdio.h>

int	error_usage(int given)
{
	fprintf(stderr, "codexion: expected 8 arguments, got %d\n", given);
	fprintf(stderr, "usage: ./codexion <number_of_coders> <time_to_burnout> "
		"<time_to_compile>\n"
		"       <time_to_debug> <time_to_refactor>\n"
		"       <number_of_compiles_required> <dongle_cooldown> <fifo|edf>\n"
		"  times are in milliseconds; <number_of_coders> and\n"
		"  <number_of_compiles_required> must be at least 1\n");
	return (1);
}

int	error_arg(const char *name, const char *value, const char *why)
{
	fprintf(stderr, "codexion: invalid <%s>: \"%s\" %s\n", name, value, why);
	return (1);
}

int	error_sys(const char *what)
{
	fprintf(stderr, "codexion: %s failed\n", what);
	return (1);
}

int	error_sys_id(const char *what, int id)
{
	fprintf(stderr, "codexion: %s %d failed\n", what, id);
	return (1);
}
