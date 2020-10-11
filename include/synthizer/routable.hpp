#pragma once

#include "synthizer/base_object.hpp"
#include "synthizer/config.hpp"
#include "synthizer/router.hpp"

#include <memory>

namespace synthizer {

/*
 * These classes are BaseObject, but for things that want to participate in routing as either a reader or a writer.
 * 
 * Writers are sources. Readers are effects.
 * 
 * At the moment, Synthizer only supports routing one level, so there's no way to be both.
 * */
class RouteWriter: public BaseObject {
	public:
	RouteWriter(const std::shared_ptr<Context> &ctx);

	router::WriterHandle *getWriterHandle() override {
		return &this->writer_handle;
	}

	private:
	router::WriterHandle writer_handle;
};

class RouteReader: public BaseObject {
	public:
	RouteReader(const std::shared_ptr<Context> &ctx, AudioSample *buffer, unsigned int channels);

	router::ReaderHandle *getReaderHandle() override {
		return &this->reader_handle;
	}

	private:
	router::ReaderHandle reader_handle;
};

}