/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2013 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef PLCRASH_FRAMEWALKER_H
#define PLCRASH_FRAMEWALKER_H

#import <sys/ucontext.h>
#import <pthread.h>

#import <stdint.h>
#import <stdbool.h>
#import <unistd.h>

#import <mach/mach.h>

#include "ApigeePLCrashAsyncThread.h"
#include "ApigeePLCrashAsyncImageList.h"

/* Configure supported targets based on the host build architecture. There's currently
 * no deployed architecture on which simultaneous support for different processor families
 * is required (or supported), but -- in theory -- such cross-architecture support could be
 * enabled by modifying these defines. */
#if defined(__i386__) || defined(__x86_64__)
#define PLFRAME_X86_SUPPORT 1
#include <mach/i386/thread_state.h>
#endif

#if defined(__arm__) || defined(__arm64__)
#define PLFRAME_ARM_SUPPORT 1
#include <mach/arm/thread_state.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @internal
 * @defgroup plframe_backtrace Backtrace Frame Walker
 * @ingroup plcrash_internal
 *
 * Implements a portable backtrace API. The API is fully async safe, and may be called
 * from any signal handler.
 *
 * The API is modeled on that of the libunwind library.
 *
 * @{
 */

/**
 * @internal
 * @defgroup plcrash_backtrace_private Internal API
 * @ingroup plframe_backtrace
 *
 * API private to the frame walker implementation.
 *
 * @{
 */

/**
 * Error return codes.
 */
typedef enum  {
    /** Success */
    APIGEE_PLFRAME_ESUCCESS = 0,

    /** Unknown error (if found, is a bug) */
    APIGEE_PLFRAME_EUNKNOWN,

    /** No more frames */
    APIGEE_PLFRAME_ENOFRAME,

    /** Bad frame */
    APIGEE_PLFRAME_EBADFRAME,

    /** Unsupported operation */
    APIGEE_PLFRAME_ENOTSUP,

    /** Invalid argument */
    APIGEE_PLFRAME_EINVAL,

    /** Internal error */
    APIGEE_PLFRAME_INTERNAL,

    /** Bad register number */
    APIGEE_PLFRAME_EBADREG
} apigee_plframe_error_t;

/**
 * @internal
 *
 * The current stack frame data
 */
typedef struct apigee_plframe_stackframe {
    /** Thread state */
    apigee_plcrash_async_thread_state_t thread_state;
} apigee_plframe_stackframe_t;

/**
 * @internal
 * Frame cursor context.
 */
typedef struct apigee_plframe_cursor {
    /** The task in which the thread stack resides */
    task_t task;
    
    /** The task's current image list. This is a borrowed reference, and must remain valid for the lifetime of the cursor. */
    apigee_plcrash_async_image_list_t *image_list;
    
    /** The current frame depth. If the depth is 0, the cursor has not been stepped, and the remainder of this
     * structure should be considered uninitialized. */
    uint32_t depth;
    
    /** The previous frame. This value is unitialized if no previous frame exists (eg, a depth of <= 1) */
    apigee_plframe_stackframe_t prev_frame;

    /** The current stack frame data */
    apigee_plframe_stackframe_t frame;
} apigee_plframe_cursor_t;

/**
 * Fetch the caller's stack frame, based on the current state in @a current_frame and @a previous_frame.
 *
 * @param task The task containing the target frame stack.
 * @param image_list The list of images loaded in the target @a task.
 * @param current_frame The current stack frame.
 * @param previous_frame The previous stack frame, or NULL if this is the first frame.
 * @param next_frame The new frame to be initialized.
 *
 * @return Returns PLFRAME_ESUCCESS on success, PLFRAME_ENOFRAME is no additional frames are available, or a standard plframe_error_t code if an error occurs.
 */
typedef apigee_plframe_error_t apigee_plframe_cursor_frame_reader_t (task_t task,
                                                       apigee_plcrash_async_image_list_t *image_list,
                                                       const apigee_plframe_stackframe_t *current_frame,
                                                       const apigee_plframe_stackframe_t *previous_frame,
                                                       apigee_plframe_stackframe_t *next_frame);

const char *apigee_plframe_strerror (apigee_plframe_error_t error);

apigee_plframe_error_t apigee_plframe_cursor_init (apigee_plframe_cursor_t *cursor, task_t task, apigee_plcrash_async_thread_state_t *thread_state, apigee_plcrash_async_image_list_t *image_list);
apigee_plframe_error_t apigee_plframe_cursor_thread_init (apigee_plframe_cursor_t *cursor, task_t task, thread_t thread, apigee_plcrash_async_image_list_t *image_list);

char const *apigee_plframe_cursor_get_regname (apigee_plframe_cursor_t *cursor, apigee_plcrash_regnum_t regnum);
size_t apigee_plframe_cursor_get_regcount (apigee_plframe_cursor_t *cursor);
apigee_plframe_error_t apigee_plframe_cursor_get_reg (apigee_plframe_cursor_t *cursor, apigee_plcrash_regnum_t regnum, apigee_plcrash_greg_t *reg);

apigee_plframe_error_t apigee_plframe_cursor_next (apigee_plframe_cursor_t *cursor);
apigee_plframe_error_t apigee_plframe_cursor_next_with_readers (apigee_plframe_cursor_t *cursor, apigee_plframe_cursor_frame_reader_t *readers[], size_t reader_count);

void apigee_plframe_cursor_free(apigee_plframe_cursor_t *cursor);

/**
 * @} plcrash_framewalker
 */
    
#ifdef __cplusplus
}
#endif

#endif /* PLCRASH_FRAMEWALKER_H */
