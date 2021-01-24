#pragma once

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <vector>

namespace synthizer {

/*
 * Docks: a multithreaded container which is lockfree on the reader path.
 * 
 * Read this comment carefully.
 * 
 * Docks are so named because they are what things moor to when the reader side goes to walk it.
 * In audio callbacks, it is necessary to have a lockfree list that avoids any overhead that can possibly be avoided, but often the order of that list doesn't matter.
 * Additionally, it is often the case that the list won't grow indefinitely, because if it did processing power would grow as O(n) or worse with the list.
 * Docks use the latter to solve the former, while avoiding ABA issues.
 * 
 * A dock defines 3 primary operations:
 * 
 * Dock: attach an object (via a shared_ptr) to the dock. We don't maintain a reference; when the shared_ptr dies, the object undocks.
 * Undock: detach an object from the dock.
 * Walk: Call a closure on all docked objects.
 * 
 * And two secondary operations:
 * 
 * disable: Disable a docked object. The object will be docked, but skipped when the dock is walked.
 * enable: Re-enable a disabled object.
 * 
 * Docks are currently MPSC: only one thread should be walking the dock at a time,.
 *
 * Any changes made in the walker need to synchronize out via another mechanism. SpscRing for instance  has its own semaphore and atomics.
 * 
 * It is important to note that eventual consistency is the only API-level guarantee: specifically, when all consumers stop calling dock/undock on the same object, the latest operation eventually wins. Walkers may not see all changes, or see the changes in the same order.
 * That said, the current implementation provides stronger guarantees. Don't rely on this. We might have to optimize later, and it will be a pain if something does.
 * 
 * And once more: docks DO NOT keep shared_ptrs alive.
 * */
template<typename element_t>
class Dock {
	private:

	struct DockNode {
		std::weak_ptr<element_t> item;
		/* We don't need atomics because nodes remain part of the list permanently. */
		DockNode *next = nullptr;
		/* Used to avoid materializing weak_ptr unnecessarily. */
		std::atomic<element_t*> item_raw = nullptr;
		std::atomic<int> enabled = 1;
		std::atomic<int> docked = 0;
	};

	std::atomic<DockNode*> head = nullptr;
	/* To track what needs freeing later. */
	std::vector<DockNode*> roots;
	/* Only ever held by dock and undock, never by walk. */
	std::mutex mutex;

	/* Needs to be called within the mutex. */
	void addPage() {
		DockNode *page = new DockNode[32];
		for(int i = 1; i < 32; i++)
			page[i].next = &page[i-1];
		page[0].next = this->head.load(std::memory_order_relaxed);
		this->head.store(&page[31], std::memory_order_release);
		roots.push_back(page);
	}

	/* Return early if func returns true. */
	template<typename T>
	void walkInternal(T &&func) {
		for(auto cur = this->head.load(std::memory_order_acquire); cur != nullptr; cur = cur->next) {
			if (func(cur) == true)
				return;
		}
	}

	template<typename T>
	void callOnNodeFor(std::shared_ptr<element_t> &ptr, T&& func) {
		auto raw = ptr.get();
		walkInternal( [&] (DockNode *cur) {
			if (raw != cur->item_raw.load(std::memory_order_relaxed))
				return false;
			auto strong = cur->item.lock();
			if (strong == ptr) {
				func(cur);
				return true;
			}
			return false;
		});
	}

	public:
	Dock() = default;

	~Dock() {
		for(auto x: this->roots) delete[] x;
	}

	/* Dock an object. If the object is already docked, do nothing. */
	void dock(std::shared_ptr<element_t> &ptr) {
		if (ptr == nullptr)
			return;

		std::lock_guard<std::mutex> g{this->mutex};
		element_t *raw = ptr.get();

		bool has = false;
		this->walkInternal( [&] (DockNode *cur) {
			if (cur->item_raw.load(std::memory_order_relaxed) != raw)
				return false;
			auto strong = cur->item.lock();
			if (strong != ptr)
				return false;
			has = cur->docked.load();
			return true;
		});

		if (has) {
			return;
		}

		bool done = false;
		while(done == false) {
			walkInternal([&](DockNode *cur) {
				if (cur->docked.load(std::memory_order_acquire) == 0) {
					cur->item = ptr;
					cur->item_raw.store(raw, std::memory_order_relaxed);
					cur->docked.store(1, std::memory_order_release);
					cur->enabled.store(1, std::memory_order_release);
					done = true;
					return true;
				}
				return false;
			});

			if (done == false)
				this->addPage();
		}
		assert(this->head.load());
	}

	/* Undock an object. If the object is already undocked, do nothing. */
	void undock(std::shared_ptr<element_t> &ptr) {
		std::lock_guard<std::mutex> g{this->mutex};
		callOnNodeFor(ptr, [&] (DockNode *cur) {
			cur->docked.store(0, std::memory_order_relaxed);
		});
	}

	/* Func is void (element_t &) */
	template<typename T>
	void walk(T &&func) {
		walkInternal([&](DockNode *cur) {
			if (cur->docked.load(std::memory_order_acquire) == 0)
				return false;
			if (cur->enabled.load(std::memory_order_relaxed) == 0)
				return false;
			auto strong = cur->item.lock();
			if (strong)
				func(*strong);
			return false;
		});
	}

	/* Enable an object. If not docked, do nothing. */
	void enable(std::shared_ptr<element_t> &ptr) {
		std::lock_guard<std::mutex> g{this->mutex};
		callOnNodeFor(ptr, [&] (DockNode *cur) {
			cur->enabled.store(1, std::memory_order_relaxed);
		});
	}

	/* Disable an object. If not docked, do nothing. */
	void disable(std::shared_ptr<element_t> &ptr) {
		std::lock_guard<std::mutex> g{this->mutex};
		callOnNodeFor(ptr, [&] (DockNode *cur) {
			cur->enabled.store(0, std::memory_order_relaxed);
		});
	}
};

}
