#pragma once

#include "synthizer/config.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/types.hpp"

namespace synthizer {

namespace router {

/*
 * The router handles routing between sources and effects. Currently this means a one-to-many mapping of one level, but the below API
 * should be designed to be extensible enough to expand this to a full DAG later (i.e. echo feeds reverb). People will want that, but for V1
 * we choose to defer that functionality: to do it the router needs to start taking ownership of executing resources
 * and we'd have to implement a full directed acyclic graph, so we avoid it for now.
 * 
 * This is called a router because all the mixing actually happens outside of it: all this knows how to do is to hand out
 * buffers, such that one side is the reader and one side is the writer.
 * 
 * To make this work, we introduce the concept of ReaderHandle and WriterHandle, which represent the two sides of the relationship: a WriterHandle is used by a source
 * to feed any number of ReaderHandles.  Higher level Synthizer components will grab the ReaderHandle and WriterHandle of the lower level pieces they wish to route, find the router, and use APIs on it to do so.
 * 
 * Handles are immovable and use their addresses to uniquely identify themselves.
 * 
 * None of the components here are threadsafe.  It's anticipated that things will go through the normal command mechanism for now, with a queue of router-specific messages later.
 * */
class Router;

/*
 * A ReaderHandle is the reader side: i.e. an effect.
 * 
 * This is created pointed at a router and holds permanent configuration on the characteristics of the buffer.
 * */
class ReaderHandle {
	public:
	/* Configure the buffer to which audio is being routed. */
	ReaderHandle(Router *router, AudioSample *buffer, unsigned int channels);
	~ReaderHandle();
	ReaderHandle(const ReaderHandle &) = delete;

	private:
	friend class Router;
	/* NOTE: Routers set the routers in their handles to NULL on shutdown to avoid the overhead of deling with weak_ptr/shared_ptr. */
	Router *router = nullptr;
	AudioSample *buffer = nullptr;
	unsigned int channels = 0;
};

/*
 * The writer is the source, etc.
 * */
class WriterHandle {
	public:
	WriterHandle(Router *router);
	~WriterHandle();
	WriterHandle(const WriterHandle &) = delete;

	/*
	 * Given an input buffer of audio data, route it to the appropriate destinations.
	 * */
	void routeAudio(AudioSample *buffer, unsigned int channels);

	private:
	friend class Router;
	/* NOTE: Routers set the routers in their handles to NULL on shutdown to avoid the overhead of deling with weak_ptr/shared_ptr. */
	Router *router = nullptr;
};

enum class RouteState {
	/* The route was created and has requested a fade-in. */
	FadeIn,
	/* The route is fading out, and will die. */
	FadeOut,
	/* The route has reached a steady state. */
 Steady,
 /* The gain has been changed, but the router isn't fading in or out. */
 GainChanged,
 /* The route has died. Remove it on the next mainloop iteration. */
 Dead,
};

/*
 * Internal type for an audio route.
 * */
class Route {
	public:
	/* Whether configuration changes should take effect. Goes to false if this route is in the process of dying. */
	bool canConfigure();
	void setGain(float gain, unsigned int time_block);
	void setState(RouteState state, unsigned int block_time);

	ReaderHandle *reader = nullptr;
	WriterHandle *writer = nullptr;
	RouteState state = RouteState::Dead;
	/* A router-local per-block timestamp of this route's last state transition time. */
	unsigned int last_state_changed = 0;
	/* When fading in, how many blocks should we fade in over? Because Synthizer is small, we keep this simple and use whole blocks only, for now. */
	unsigned int fade_in_blocks = 1;
	/* Same as fade_in_blocks but for fade-out. */
	unsigned int fade_out_blocks = 1;
	/* When we're in the steady state, what should our gain be? */
	float gain = 1.0f;
	/* When the gain just changed, what was the previous value? */
	float prev_gain = 1.0f;
};

class Router {
	public:
	~Router();
	Router(const Router&) = delete;

	/*
	 * Establish or update a route from WriteHandle w to ReadHandle r with specified gain and fade-in.
	 * View this as a declarative interface. if w->r doesn't exist, it gets added, with the specified fadein time, otherwise the gain is updated.
	 * */
	void configureRoute(WriterHandle *w, ReaderHandle *r, float gain = 1.0, unsigned int fade_in = 1);
	/* Remove a route. If it doesn't exist, do nothing. */
	void removeRoute(WriterHandle *w, ReaderHandle *r, unsigned int fade_out = 1);
	/* Remove all routes for a specified WriteHandle. */
	void removeAllRoutes(WriterHandle *w, unsigned int fade_out = 1);
	/* Signal the router that we are finished with a block of audio. Increments the internal timestamp. */
	void finishBlock();

	private:
	friend class ReaderHandle;
	friend class WriterHandle;

	void unregisterReaderHandle(ReaderHandle *handle);
	void unregisterWriterHandle(WriterHandle *h);

	/*
	 * Filter out any route which is dead or which has either the reader or writer specified.
	 * Since no route ever has a null reader or writer, using nulls as sentinel values here is fine and lets this function
	 * cover all the cases.
	 * */
	void filterRoutes(WriterHandle *w, ReaderHandle *r);

 /* Returns routes.end() if not found. */
	deferred_vector<Route>::iterator findRouteForPair(WriterHandle *writer, ReaderHandle *reader);

	/* Returns iterator to the beginning of the runfor the specified writer. */
	deferred_vector<Route>::iterator findRun(WriterHandle *writer);

	/* How often, in blocks, to call filterRoutes ourselves to get rid of anything that might be dead. */
	static const unsigned int FILTER_BLOCK_COUNT = 10;
	deferred_vector<Route> routes;
	unsigned int time = 0;
};

} /* namespace router */
} /* namespace synthizer */