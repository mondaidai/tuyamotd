#include "json_writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define JSON_MAX_DEPTH 16

static int append(json_writer_t *w, const char *fmt, ...)
{
	if(!w || !fmt || w->failed)
		return -1;

	va_list ap;
	va_start(ap, fmt);

	int rc = vsnprintf(w->buf + w->len, w->size - w->len, fmt, ap);
	va_end(ap);

	if(rc < 0 || (size_t)rc >= (w->size - w->len))
	{
		w->failed = true;
		return -1;
	}

	w->len += (size_t)rc;
	return 0;
}

static int separator(json_writer_t *w)
{
	if(w->depth < 0)
		return -1;

	if(w->need_comma[w->depth])
	{
		if(append(w, ","))
			return -1;
	}

	w->need_comma[w->depth] = true;
	return 0;
}

static int escape_string(json_writer_t * w, const char * str)
{
	if(!str)
		str = "";

	if(append(w, "\""))
		return -1;

	while (*str)
	{
		unsigned char c = (unsigned char)*str;

		switch (c)
		{
		case '\"':
			if(append(w, "\\\""))
				return -1;
			break;

		case '\\':
			if(append(w, "\\\\"))
				return -1;
			break;

		case '\b':
			if(append(w, "\\b"))
				return -1;
			break;

		case '\f':
			if(append(w, "\\f"))
				return -1;
			break;

		case '\n':
			if(append(w, "\\n"))
				return -1;
			break;

		case '\r':
			if(append(w, "\\r"))
				return -1;
			break;

		case '\t':
			if(append(w, "\\t"))
				return -1;
			break;

		default:
			if(c < 0x20)
			{
				if(append(w, "\\u%04x", c))
					return -1;
			}
			else
			{
				if(append(w, "%c", c))
					return -1;
			}
			break;
		}

		str++;
	}

	return append(w, "\"");
}

void json_init(json_writer_t * w, char *buffer, size_t size)
{
	memset(w, 0, sizeof(*w));

	w->buf = buffer;
	w->size = size;

	if(buffer && size)
		buffer[0] = '\0';
	return;
}

int json_object_begin(json_writer_t *w)
{
	if(separator(w))
		return -1;

	if(append(w, "{"))
		return -1;

	if(++w->depth >= JSON_MAX_DEPTH)
	{
		w->failed = true;
		return -1;
	}

	w->need_comma[w->depth] = false;

	return 0;
}

int json_object_end(json_writer_t *w)
{
	w->depth--;
	return append(w, "}");
}

int json_object_field_begin(json_writer_t *w, const char *name)
{
	if(separator(w))
		return -1;

	if(escape_string(w, name))
		return -1;

	if(append(w, ":{"))
		return -1;

	if(++w->depth >= JSON_MAX_DEPTH)
	{
		w->failed = true;
		return -1;
	}

	w->need_comma[w->depth] = false;
	return 0;
}

int json_string(json_writer_t * w, const char * name, const char * value)
{
	if(separator(w))
		return -1;

	if(escape_string(w, name))
		return -1;

	if(append(w, ":"))
		return -1;

	return escape_string(w, value);
}

int json_u64(json_writer_t * w, const char * name, uint64_t value)
{
	if(separator(w))
		return -1;

	if(escape_string(w, name))
		return -1;

	return append(w, ":%llu", (unsigned long long)value);
}

int json_array_begin(json_writer_t * w, const char * name)
{
	if(separator(w))
		return -1;

	if(escape_string(w, name))
		return -1;

	if(append(w, ":["))
		return -1;

	if(++w->depth >= JSON_MAX_DEPTH)
	{
		w->failed = true;
		return -1;
	}

	w->need_comma[w->depth] = false;

	return 0;
}

int json_array_end(json_writer_t * w)
{
	w->depth--;
	return append(w, "]");
}

int json_array_field_begin(json_writer_t *w, const char *name)
{
	if(separator(w))
		return -1;

	if(escape_string(w, name))
		return -1;

	if(append(w, ":["))
		return -1;

	if(++w->depth >= JSON_MAX_DEPTH)
	{
		w->failed = true;
		return -1;
	}

	w->need_comma[w->depth] = false;
	return 0;
}

int json_object_element_begin(json_writer_t * w)
{
	if(separator(w))
		return -1;

	if(append(w, "{"))
		return -1;

	if(++w->depth >= JSON_MAX_DEPTH)
	{
		w->failed = true;
		return -1;
	}

	w->need_comma[w->depth] = false;

	return 0;
}

int json_string_element(json_writer_t * w, const char * value)
{
	if(separator(w))
		return -1;

	return escape_string(w, value);
}

int json_raw_element(json_writer_t *w, const char *value)
{
	if(separator(w))
		return -1;

	return append(w, "%s", value ? value : "");
}

bool json_ok(const json_writer_t *w)
{
	return w && !w->failed;
}