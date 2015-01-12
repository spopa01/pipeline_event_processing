#ifndef __EVENT_H__
#define __EVENT_H__

#include <memory>

//EVENT DEFINITION

class event_t;
using event_sptr_t = std::shared_ptr< event_t >;

class event_t {
public:
  event_t() {}
	virtual ~event_t() {}
	virtual int get_type() const = 0;
};

//CONTROL EVENTS DEFINITION

class start_event_t : public event_t {
public:
	static int type() { return 1; }
	int get_type() const override { return type(); }
};

class stop_event_t : public event_t {
public:
	static int type() { return 2; }
	int get_type() const override { return type(); }
};

#endif//__EVENT_H__
