/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#ifndef _ARM_PMAP_PUBLIC_H_
#define _ARM_PMAP_PUBLIC_H_

#include <stddef.h>
#include <mach/kern_return.h>
#include <mach/vm_types.h>
#include <mach/vm_prot.h>
#include <arm64/sptm/sptm.h>

__BEGIN_DECLS

#if defined(__arm64__)
typedef uint64_t pmap_paddr_t __kernel_ptr_semantics; /* physical address (not ppnum_t) */
#else
typedef uint32_t pmap_paddr_t __kernel_ptr_semantics; /* physical address (not ppnum_t) */
#endif


__END_DECLS

#endif /* _ARM_PMAP_PUBLIC_H_ */
