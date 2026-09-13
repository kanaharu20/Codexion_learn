/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parse.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hkanamit <hkanamit@student.42tokyo.jp>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/11 16:30:00 by hkanamit          #+#    #+#             */
/*   Updated: 2026/09/11 16:30:00 by hkanamit         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "header.h"
#include <limits.h>
#include <string.h>

static int	is_valid_num(const char *s)
{
	int	i;

	if (!s[0])
		return (0);
	i = 0;
	while (s[i])
	{
		if (s[i] < '0' || '9' < s[i])
			return (0);
		i++;
	}
	return (1);
}

static int	ori_atoi(const char *str, int *arg)
{
	int	i;
	int	num;
	int	digit;

	i = 0;
	num = 0;
	while (str[i])
	{
		digit = str[i] - '0';
		if (num > (INT_MAX - digit) / 10)
			return (0);
		num = num * 10 + digit;
		i++;
	}
	*arg = num;
	return (1);
}

static int	parse_one(const char *s, int *dst, const char *name)
{
	if (!is_valid_num(s))
		return (error_arg(name, s,
				"must be a non-negative integer (digits only, no sign)"));
	if (!ori_atoi(s, dst))
		return (error_arg(name, s, "does not fit in int (max 2147483647)"));
	return (0);
}

static int	check_ranges(char **argv, t_args *ins)
{
	if (ins->num_coders < 1)
		return (error_arg("number_of_coders", argv[1], "must be at least 1"));
	if (ins->num_compile_req < 1)
		return (error_arg("number_of_compiles_required", argv[6],
				"must be at least 1"));
	return (0);
}

int	parse_args(char **argv, t_args *ins)
{
	if (parse_one(argv[1], &ins->num_coders, "number_of_coders") != 0
		|| parse_one(argv[2], &ins->t_to_burnout, "time_to_burnout") != 0
		|| parse_one(argv[3], &ins->t_to_compile, "time_to_compile") != 0
		|| parse_one(argv[4], &ins->t_to_debug, "time_to_debug") != 0
		|| parse_one(argv[5], &ins->t_to_refactor, "time_to_refactor") != 0
		|| parse_one(argv[6], &ins->num_compile_req,
			"number_of_compiles_required") != 0
		|| parse_one(argv[7], &ins->dongle_cooldown, "dongle_cooldown") != 0)
		return (1);
	if (check_ranges(argv, ins) != 0)
		return (1);
	if (strcmp(argv[8], "fifo") != 0 && strcmp(argv[8], "edf") != 0)
		return (error_arg("scheduler", argv[8],
				"must be either fifo or edf"));
	ins->scheduler = argv[8];
	return (0);
}
