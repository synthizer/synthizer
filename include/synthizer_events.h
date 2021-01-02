#pragma once

#include "synthizer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct syz_EventFinished {
	char unused; // gives the struct a size in C, lets C->C++ not die horribly.
};

struct syz_EventLooped {
	unsigned long long loop_counter;
};

struct syz_Event {
	int type;
	syz_Handle source;
	void *userdata;
	union {
	 struct syz_EventLooped looped;
	 struct syz_EventFinished finished;
	} payload;
};

#ifdef __cplusplus
}
#endif
