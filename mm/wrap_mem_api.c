/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/dmapool.h>
#include <linux/mm.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <asm/page.h>
#include <asm/stacktrace.h>

#include "slab.h"

struct obj_walking {
	int pos;
	void **d;
};

static int walkstack(struct stackframe *frame, void *p)
{
	struct obj_walking *w = (struct obj_walking *)p;
	unsigned long pc = frame->pc;

	if (w->pos < 9) {
		w->d[w->pos++] = (void *)pc;
		return 0;
	}

	return 1;
}

static void get_stacktrace(void **stack)
{
	struct stackframe frame;
	struct obj_walking w = {0, stack};
	void *p = &w;

	register unsigned long current_sp asm ("sp");
	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.sp = current_sp;
	frame.pc = (unsigned long)get_stacktrace;

	walk_stackframe(&frame, walkstack, p);
}

void *__wrap___kmalloc(size_t size, gfp_t flags)
{
	void *stack[9] = {0};
	void *addr = (void *)__kmalloc(size, flags);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap___kmalloc);

void __wrap_kfree(const void *block)
{
	void *addr = (void *)block;

	if (block)
		debug_object_trace_free(addr);
	kfree(block);

	return;
}
EXPORT_SYMBOL(__wrap_kfree);

void *__wrap_devm_kmalloc(struct device *dev, size_t size, gfp_t gfp)
{
	void *stack[9] = {0};
	void *addr = (void *)devm_kmalloc(dev, size, gfp);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL_GPL(__wrap_devm_kmalloc);

void *__wrap_devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	void *stack[9] = {0};
	void *addr = (void *)devm_kzalloc(dev, size, gfp);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_devm_kzalloc);

void *__wrap_devm_kmalloc_array(struct device *dev, size_t n,
				size_t size, gfp_t gfp)
{
	void *stack[9] = {0};
	void *addr = (void *)devm_kmalloc_array(dev, n, size, gfp);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, n*size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_devm_kmalloc_array);

void *__wrap_devm_kcalloc(struct device *dev, size_t n,
				size_t size, gfp_t gfp)
{
	void *stack[9] = {0};
	void *addr = (void *)devm_kcalloc(dev, n, size, gfp);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, n*size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_devm_kcalloc);

void __wrap_devm_kfree(struct device *dev, void *p)
{
	debug_object_trace_free(p);
	devm_kfree(dev, p);

	return;
}
EXPORT_SYMBOL_GPL(__wrap_devm_kfree);

void *__wrap_devm_kmemdup(struct device *dev, const void *src,
					size_t len, gfp_t gfp)
{
	void *stack[9] = {0};
	void *addr = (void *)devm_kmemdup(dev, src, len, gfp);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, len);
	}

	return addr;
}
EXPORT_SYMBOL_GPL(__wrap_devm_kmemdup);

unsigned long __wrap_devm_get_free_pages(struct device *dev,
			gfp_t gfp_mask, unsigned int order)
{
	void *stack[9] = {0};
	void *addr = (void *)devm_get_free_pages(dev, gfp_mask, order);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, (1 << order) * PAGE_SIZE);
	}

	return (unsigned long)addr;
}
EXPORT_SYMBOL_GPL(__wrap_devm_get_free_pages);

void __wrap_devm_free_pages(struct device *dev, unsigned long addr)
{
	debug_object_trace_free((void *)addr);
	devm_free_pages(dev, addr);

	return;
}
EXPORT_SYMBOL_GPL(__wrap_devm_free_pages);

struct page *__wrap_alloc_pages(gfp_t gfp_mask,
			unsigned int order)
{
	void *stack[9] = {0};
	struct page *page = alloc_pages(gfp_mask, order);

	if (page) {
		get_stacktrace(stack);
		debug_object_trace_init(page_address(page), stack,
					(1 << order) * PAGE_SIZE);
	}

	return page;
}
EXPORT_SYMBOL(__wrap_alloc_pages);

struct page *__wrap_alloc_pages_node(int node, gfp_t gfp_mask,
			unsigned int order)
{
	void *stack[9] = {0};
	struct page *page = alloc_pages_node(node, gfp_mask, order);

	if (page) {
		get_stacktrace(stack);
		debug_object_trace_init(page_address(page), stack,
					(1 << order) * PAGE_SIZE);
	}

	return page;
}
EXPORT_SYMBOL(__wrap_alloc_pages_node);

unsigned long __wrap___get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	void *stack[9] = {0};
	void *addr = (void *)__get_free_pages(gfp_mask, order);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, (1 << order) * PAGE_SIZE);
	}

	return (unsigned long)addr;
}
EXPORT_SYMBOL(__wrap___get_free_pages);

unsigned long __wrap_get_zeroed_page(gfp_t gfp_mask)
{
	void *stack[9] = {0};
	void *addr = (void *)get_zeroed_page(gfp_mask);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, 1 * PAGE_SIZE);
	}

	return (unsigned long)addr;
}
EXPORT_SYMBOL(__wrap_get_zeroed_page);

void *__wrap_dma_alloc_coherent(struct device *dev, size_t size,
			dma_addr_t *handle, gfp_t gfp)
{
	void *stack[9] = {0};
	void *addr = (void *)dma_alloc_coherent(dev, size, handle, gfp);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_dma_alloc_coherent);


void __wrap_dma_free_coherent(struct device *dev, size_t size,
			void *cpu_addr, dma_addr_t dma_handle)
{
	debug_object_trace_free(cpu_addr);
	dma_free_coherent(dev, size, cpu_addr, dma_handle);

	return;
}
EXPORT_SYMBOL(__wrap_dma_free_coherent);

void *__wrap_kmem_cache_alloc(struct kmem_cache *s, gfp_t gfpflags)
{
	void *stack[9] = {0};
	void *addr = (void *)kmem_cache_alloc(s, gfpflags);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, s->size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_kmem_cache_alloc);

void __wrap_kmem_cache_free(struct kmem_cache *s, void *x)
{
	debug_object_trace_free(x);
	kmem_cache_free(s, x);

	return;
}
EXPORT_SYMBOL(__wrap_kmem_cache_free);

void *__wrap_kmalloc(size_t size, gfp_t flags)
{
	void *stack[9] = {0};
	void *addr = kmalloc(size, flags);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_kmalloc);

void *__wrap_kmalloc_array(size_t n, size_t size, gfp_t flags)
{
	void *stack[9] = {0};
	void *addr = (void *)kmalloc_array(n, size, flags);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, n*size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_kmalloc_array);

void *__wrap_kcalloc(size_t n, size_t size, gfp_t flags)
{
	void *stack[9] = {0};
	void *addr = kcalloc(n, size, flags);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, n*size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_kcalloc);

void *__wrap_kzalloc(size_t size, gfp_t flags)
{
	void *stack[9] = {0};
	void *addr = kzalloc(size, flags);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_kzalloc);

void *__wrap_kzalloc_node(size_t size, gfp_t flags, int node)
{
	void *stack[9] = {0};
	void *addr = kzalloc_node(size, flags, node);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_kzalloc_node);

void *__wrap_kmalloc_order(size_t size, gfp_t flags, unsigned int order)
{
	void *stack[9] = {0};
	void *addr = kmalloc_order(size, flags, order);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, (1 << order) * PAGE_SIZE);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_kmalloc_order);

void *__wrap_dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags,
			dma_addr_t *handle)
{
	void *stack[9] = {0};
	void *addr = (void *)dma_pool_alloc(pool, mem_flags, handle);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, pool->size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_dma_pool_alloc);

void __wrap_dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t dma)
{
	debug_object_trace_free(vaddr);
	dma_pool_free(pool, vaddr, dma);

	return;
}
EXPORT_SYMBOL(__wrap_dma_pool_free);

void *__wrap_mempool_alloc(mempool_t *pool, gfp_t gfp_mask)
{
	void *stack[9] = {0};
	void *addr = (void *)mempool_alloc(pool, gfp_mask);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, pool->min_nr);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_mempool_alloc);

void __wrap_mempool_free(void *element, mempool_t *pool)
{
	debug_object_trace_free(element);
	mempool_free(element, pool);

	return;
}
EXPORT_SYMBOL(__wrap_mempool_free);

void __wrap___free_pages(struct page *page, unsigned int order)
{
	debug_object_trace_free(page_address(page));
	__free_pages(page, order);

	return;
}
EXPORT_SYMBOL(__wrap___free_pages);

void *__wrap_kmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *stack[9] = {0};
	void *addr = (void *)kmemdup(src, len, gfp);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, len);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_kmemdup);

void *__wrap_memdup_user(const void *src, size_t len)
{
	void *stack[9] = {0};
	void *addr = (void *)memdup_user(src, len);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, len);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_memdup_user);

void __wrap_kvfree(const void *block)
{
	void *addr = (void *)block;

	debug_object_trace_free(addr);
	kvfree(block);

	return;
}
EXPORT_SYMBOL(__wrap_kvfree);

void __wrap_vfree(const void *addr)
{
	void *vptr = (void *)addr;

	debug_object_trace_free(vptr);
	vfree(addr);

	return;
}
EXPORT_SYMBOL(__wrap_vfree);

void *__wrap___vmalloc(unsigned long size, gfp_t gfp_mask, pgprot_t prot)
{
	void *stack[9] = {0};
	void *addr = (void *)__vmalloc(size, gfp_mask, prot);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap___vmalloc);

void *__wrap_vmalloc(unsigned long size)
{
	void *stack[9] = {0};
	void *addr = vmalloc(size);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_vmalloc);

void *__wrap_vzalloc(unsigned long size)
{
	void *stack[9] = {0};
	void *addr = vzalloc(size);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_vzalloc);

void *__wrap_vmalloc_user(unsigned long size)
{
	void *stack[9] = {0};
	void *addr = (void *)vmalloc_user(size);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_vmalloc_user);

void *__wrap_vmalloc_node(unsigned long size, int node)
{
	void *stack[9] = {0};
	void *addr = vmalloc_node(size, node);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_vmalloc_node);

void *__wrap_vzalloc_node(unsigned long size, int node)
{
	void *stack[9] = {0};
	void *addr = vzalloc_node(size, node);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_vzalloc_node);

void *__wrap_vmalloc_32(unsigned long size)
{
	void *stack[9] = {0};
	void *addr = (void *)vmalloc_32(size);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_vmalloc_32);

void *__wrap_krealloc(const void *p, size_t new_size, gfp_t flags)
{
	void *stack[9] = {0};
	void *addr = (void *)krealloc(p, new_size, flags);

	if (addr) {
		get_stacktrace(stack);
		debug_object_trace_init(addr, stack, new_size);
	}

	return addr;
}
EXPORT_SYMBOL(__wrap_krealloc);

void __wrap_kzfree(const void *p)
{
	void *addr = (void *)p;

	debug_object_trace_free(addr);
	kzfree(p);

	return;
}
EXPORT_SYMBOL(__wrap_kzfree);
