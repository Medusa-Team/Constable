// SPDX-License-Identifier: GPL-2.0

#include "mlibc_format.h"

#include <limits.h>
#include <stdint.h>

#define ZEROPAD	1	/* pad with zero */
#define SIGN	2	/* unsigned/signed long */
#define PLUS	4	/* show plus */
#define SPACE	8	/* space if plus */
#define LEFT	16	/* left justified */
#define SPECIAL	32	/* prefix non-decimal values */
#define LARGE	64	/* use uppercase digits */

struct format_output {
	char *buffer;
	size_t capacity;
	size_t length;
};

static int is_digit(char character)
{
	return character >= '0' && character <= '9';
}

static void output_character(struct format_output *output, char character)
{
	if (output->capacity && output->length < output->capacity - 1)
		output->buffer[output->length] = character;
	if (output->length < SIZE_MAX)
		output->length++;
}

static void output_repeat(struct format_output *output, char character,
			  int count)
{
	size_t amount;
	size_t available = 0;
	size_t index;

	if (count <= 0)
		return;
	amount = (size_t)count;
	if (output->capacity && output->length < output->capacity - 1)
		available = output->capacity - 1 - output->length;
	if (available > amount)
		available = amount;
	for (index = 0; index < available; index++)
		output->buffer[output->length + index] = character;
	if (SIZE_MAX - output->length < amount)
		output->length = SIZE_MAX;
	else
		output->length += amount;
}

static void output_string(struct format_output *output, const char *string,
			  int length)
{
	int index;

	for (index = 0; index < length; index++)
		output_character(output, string[index]);
}

static int skip_atoi(const char **string)
{
	int value = 0;

	while (is_digit(**string)) {
		if (value > (INT_MAX - (**string - '0')) / 10)
			value = INT_MAX;
		else
			value = value * 10 + **string - '0';
		(*string)++;
	}
	return value;
}

static void output_number(struct format_output *output, unsigned long value,
			  int base, int width, int precision, int flags)
{
	char padding;
	char sign = '\0';
	char temporary[sizeof(value) * CHAR_BIT + 1];
	const char *digits = (flags & LARGE) ?
		"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" :
		"0123456789abcdefghijklmnopqrstuvwxyz";
	int digits_count = 0;

	if (flags & LEFT)
		flags &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return;

	padding = (flags & ZEROPAD) ? '0' : ' ';
	if (flags & SIGN) {
		long signed_value = (long)value;

		if (signed_value < 0) {
			sign = '-';
			value = 0UL - value;
			width--;
		} else if (flags & PLUS) {
			sign = '+';
			width--;
		} else if (flags & SPACE) {
			sign = ' ';
			width--;
		}
	}

	if (flags & SPECIAL) {
		if (base == 16)
			width -= 2;
		else if (base == 8)
			width--;
	}

	do {
		temporary[digits_count++] = digits[value % (unsigned int)base];
		value /= (unsigned int)base;
	} while (value);

	if (precision < digits_count)
		precision = digits_count;
	width -= precision;

	if (!(flags & (ZEROPAD | LEFT))) {
		output_repeat(output, ' ', width);
		width = 0;
	}
	if (sign)
		output_character(output, sign);
	if (flags & SPECIAL) {
		if (base == 8) {
			output_character(output, '0');
		} else if (base == 16) {
			output_character(output, '0');
			output_character(output, digits[33]);
		}
	}
	if (!(flags & LEFT))
		output_repeat(output, padding, width);
	output_repeat(output, '0', precision - digits_count);
	while (digits_count-- > 0)
		output_character(output, temporary[digits_count]);
	if (flags & LEFT)
		output_repeat(output, ' ', width);
}

int mlibc_vsnprintf(char *buffer, size_t capacity, const char *format,
		    va_list arguments)
{
	struct format_output output = {
		.buffer = buffer,
		.capacity = capacity,
	};
	int flags;
	int width;
	int precision;
	int qualifier;
	int base;

	if ((!buffer && capacity) || !format)
		return -1;

	while (*format) {
		const char *string;
		unsigned long number;
		int length;

		if (*format != '%') {
			output_character(&output, *format++);
			continue;
		}

		flags = 0;
repeat:
		format++;
		switch (*format) {
		case '-':
			flags |= LEFT;
			goto repeat;
		case '+':
			flags |= PLUS;
			goto repeat;
		case ' ':
			flags |= SPACE;
			goto repeat;
		case '#':
			flags |= SPECIAL;
			goto repeat;
		case '0':
			flags |= ZEROPAD;
			goto repeat;
		}

		width = -1;
		if (is_digit(*format)) {
			width = skip_atoi(&format);
		} else if (*format == '*') {
			format++;
			width = va_arg(arguments, int);
			if (width < 0) {
				if (width == INT_MIN)
					width = INT_MAX;
				else
					width = -width;
				flags |= LEFT;
			}
		}

		precision = -1;
		if (*format == '.') {
			format++;
			if (is_digit(*format))
				precision = skip_atoi(&format);
			else if (*format == '*') {
				format++;
				precision = va_arg(arguments, int);
			}
			if (precision < 0)
				precision = 0;
		}

		qualifier = -1;
		if (*format == 'h' || *format == 'l' || *format == 'L') {
			qualifier = *format;
			format++;
		}

		base = 10;
		switch (*format) {
		case 'c':
			if (!(flags & LEFT))
				output_repeat(&output, ' ', width - 1);
			output_character(&output,
					 (char)(unsigned char)va_arg(arguments,
								     int));
			if (flags & LEFT)
				output_repeat(&output, ' ', width - 1);
			format++;
			continue;
		case 's':
			string = va_arg(arguments, char *);
			if (!string)
				string = "<NULL>";
			for (length = 0;
			     string[length] &&
			     (precision < 0 || length < precision);
			     length++)
				;
			if (!(flags & LEFT))
				output_repeat(&output, ' ', width - length);
			output_string(&output, string, length);
			if (flags & LEFT)
				output_repeat(&output, ' ', width - length);
			format++;
			continue;
		case 'p':
			if (width == -1) {
				width = 2 * (int)sizeof(void *);
				flags |= ZEROPAD;
			}
			output_number(&output,
				      (unsigned long)(uintptr_t)
				      va_arg(arguments, void *),
				      16, width, precision, flags);
			format++;
			continue;
		case 'n':
			if (qualifier == 'l') {
				long *value = va_arg(arguments, long *);

				*value = output.length > LONG_MAX ?
					LONG_MAX : (long)output.length;
			} else {
				int *value = va_arg(arguments, int *);

				*value = output.length > INT_MAX ?
					INT_MAX : (int)output.length;
			}
			format++;
			continue;
		case '%':
			output_character(&output, '%');
			format++;
			continue;
		case 'o':
			base = 8;
			break;
		case 'X':
			flags |= LARGE;
			/* fall through */
		case 'x':
			base = 16;
			break;
		case 'd':
		case 'i':
			flags |= SIGN;
			/* fall through */
		case 'u':
			break;
		default:
			output_character(&output, '%');
			if (*format)
				output_character(&output, *format++);
			continue;
		}

		if (qualifier == 'l') {
			if (flags & SIGN)
				number = (unsigned long)va_arg(arguments, long);
			else
				number = va_arg(arguments, unsigned long);
		} else if (qualifier == 'h') {
			number = (unsigned short)va_arg(arguments, int);
			if (flags & SIGN)
				number = (unsigned long)
					(short)(unsigned short)number;
		} else if (flags & SIGN) {
			number = (unsigned long)va_arg(arguments, int);
		} else {
			number = va_arg(arguments, unsigned int);
		}
		output_number(&output, number, base, width, precision, flags);
		format++;
	}

	if (capacity) {
		size_t terminator = output.length;

		if (terminator >= capacity)
			terminator = capacity - 1;
		buffer[terminator] = '\0';
	}
	return output.length > INT_MAX ? INT_MAX : (int)output.length;
}

int mlibc_snprintf(char *buffer, size_t capacity, const char *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = mlibc_vsnprintf(buffer, capacity, format, arguments);
	va_end(arguments);
	return result;
}
