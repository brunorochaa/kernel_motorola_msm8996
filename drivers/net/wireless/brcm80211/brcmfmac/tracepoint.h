/*
 * Copyright (c) 2013 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#if !defined(BRCMF_TRACEPOINT_H_) || defined(TRACE_HEADER_MULTI_READ)
#define BRCMF_TRACEPOINT_H_

#include <linux/types.h>
#include <linux/tracepoint.h>

#ifndef CONFIG_BRCM_TRACING

#undef TRACE_EVENT
#define TRACE_EVENT(name, proto, ...) \
static inline void trace_ ## name(proto) {}

#undef DECLARE_EVENT_CLASS
#define DECLARE_EVENT_CLASS(...)

#undef DEFINE_EVENT
#define DEFINE_EVENT(evt_class, name, proto, ...) \
static inline void trace_ ## name(proto) {}

#endif /* CONFIG_BRCM_TRACING */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM	brcmfmac

#define MAX_MSG_LEN		100

TRACE_EVENT(brcmf_err,
	TP_PROTO(const char *func, struct va_format *vaf),
	TP_ARGS(func, vaf),
	TP_STRUCT__entry(
		__string(func, func)
		__dynamic_array(char, msg, MAX_MSG_LEN)
	),
	TP_fast_assign(
		__assign_str(func, func);
		WARN_ON_ONCE(vsnprintf(__get_dynamic_array(msg),
				       MAX_MSG_LEN, vaf->fmt,
				       *vaf->va) >= MAX_MSG_LEN);
	),
	TP_printk("%s: %s", __get_str(func), __get_str(msg))
);

TRACE_EVENT(brcmf_dbg,
	TP_PROTO(u32 level, const char *func, struct va_format *vaf),
	TP_ARGS(level, func, vaf),
	TP_STRUCT__entry(
		__field(u32, level)
		__string(func, func)
		__dynamic_array(char, msg, MAX_MSG_LEN)
	),
	TP_fast_assign(
		__entry->level = level;
		__assign_str(func, func);
		WARN_ON_ONCE(vsnprintf(__get_dynamic_array(msg),
				       MAX_MSG_LEN, vaf->fmt,
				       *vaf->va) >= MAX_MSG_LEN);
	),
	TP_printk("%s: %s", __get_str(func), __get_str(msg))
);

TRACE_EVENT(brcmf_hexdump,
	TP_PROTO(void *data, size_t len),
	TP_ARGS(data, len),
	TP_STRUCT__entry(
		__field(unsigned long, len)
		__dynamic_array(u8, hdata, len)
	),
	TP_fast_assign(
		__entry->len = len;
		memcpy(__get_dynamic_array(hdata), data, len);
	),
	TP_printk("hexdump [length=%lu]", __entry->len)
);

#ifdef CONFIG_BRCM_TRACING

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE tracepoint

#include <trace/define_trace.h>

#endif /* CONFIG_BRCM_TRACING */

#endif /* BRCMF_TRACEPOINT_H_ */
