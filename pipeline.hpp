#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include "event.hpp"

#include <vector>
#include <functional>

//EVENT PROCESSOR DEFINITION

using event_processor_func_t = std::function<event_sptr_t(event_sptr_t const&)>;

//EVENT PROCESSOR PIPELINE DEFINITION

//base class
class event_processor_pipeline_t {
public:
	enum class state_t : char  { idle, running, stopped };

	event_processor_pipeline_t( event_processor_func_t && callback ) 
		: callback_{ std::forward<event_processor_func_t>(callback) },
			state_{ state_t::idle } {}
	virtual ~event_processor_pipeline_t() {}

	virtual void add_stage( event_processor_func_t && processor ) = 0;
	virtual void operator()( event_sptr_t const& event ) = 0;

	state_t get_state() const { return state_; }

protected:
	event_processor_func_t callback_;
	void set_state( state_t state ){ state_ = state; }

private:
	state_t state_;
};

using event_processor_pipeline_sptr_t = std::shared_ptr<event_processor_pipeline_t>;

//serial pipeline

class serial_event_processor_pipeline_t : public event_processor_pipeline_t {
public:
  serial_event_processor_pipeline_t( event_processor_func_t && callback );

  void add_stage( event_processor_func_t && processor ) override;
  void operator()( event_sptr_t const& event ) override;

private:
  std::vector< event_processor_func_t > stages_;
};

//tbb pipeline
class tbb_event_processor_pipeline_t : public event_processor_pipeline_t {
public:
  tbb_event_processor_pipeline_t( event_processor_func_t && callback, size_t live_tokens );
  ~tbb_event_processor_pipeline_t();

	void add_stage( event_processor_func_t && processor ) override;
	void operator()( event_sptr_t const& event ) override;

private:
	class impl_t;
	std::unique_ptr<impl_t> impl_;
};

#endif//__PIPELINE_H__
