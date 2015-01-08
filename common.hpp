#ifndef __COMMON_H__
#define __COMMON_H__

#include <memory>
#include <vector>
#include <functional>

//EVENT DEFINITION

class event_t;
using event_sptr_t = std::shared_ptr< event_t >;

class event_t {
public:
  event_t() {}
  virtual ~event_t() {}

  virtual int get_type() = 0;
};

//CONTROL EVENTS

class start_event_t : public event_t {
public:
  static const int type;
  int get_type() override { return type; }
};

class stop_event_t : public event_t {
public:
  static const int type;
  int get_type() override { return type; }
};

//EVENT PROCESSOR DEFINITION

class event_processor_t;
using event_processor_sptr_t = std::shared_ptr< event_processor_t >;

using event_processor_func_t = std::function<event_sptr_t(event_sptr_t const&)>;

class event_processor_t {
public:
  event_processor_t( event_processor_func_t&& processor )
		: processor_{std::forward<event_processor_func_t>(processor)} {}

  event_processor_t( event_processor_sptr_t const& callback, event_processor_func_t&& processor ) 
		: callback_{callback}, processor_{std::forward<event_processor_func_t>(processor)} {}

	void set_callback(event_processor_sptr_t const& callback) { callback_ = callback; }
	
	void set_processor(event_processor_func_t const& processor) { processor_ = processor; }

  virtual void operator()( event_sptr_t const& event ) { 
		if( processor_ ){ 
			auto computed_event = processor_(event);
			if(callback_)
				callback_( computed_event );
		}
	}

protected:
  event_processor_func_t processor_;
	event_processor_sptr_t callback_;
};

//EVENT PROCESSOR PIPELINE DEFINITION

//base class
class event_processor_pipeline_t : public event_processor_t {
public:
  enum class state_t : char  { idle, running, stopped };

  event_processor_pipeline_t( event_callback_t && callback ) 
		: callback_{ std::forward<event_callback_t>(callback) }
			state_{ state_t::idle; }
  
	virtual void add_stage( event_processor_sptr_t const& event_processor ) = 0;

  state_t get_state() const { return state_; }

protected:
  
	void set_state( state_t state ){ state_ = state; }

private:
  state_t state_;
};

using event_processor_pipeline_sptr_t = std::shared_ptr<event_processor_pipeline_t>;

//simple pipeline
class simple_event_processor_pipeline_t : public event_processor_pipeline_t {
public:
  simple_event_processor_pipeline_t( event_callback_t && callback );

  void add_stage( event_processor_sptr_t const& event_processor ) override;

  event_sptr_t operator()( event_sptr_t const& event ) override;

private:
  std::vector< event_processor_sptr_t > stages_;
};

//tbb pipeline
class tbb_event_processor_pipeline_t : public event_processor_pipeline_t {
public:
  tbb_event_processor_pipeline_t( event_callback_t && callback, size_t live_tokens );
  ~tbb_event_processor_pipeline_t();

  void add_stage( event_processor_sptr_t const& event_processor ) override;

  event_sptr_t operator()( event_sptr_t const& event ) override;

private:
  class impl_t;
  std::unique_ptr<impl_t> impl_;
};

#endif//__COMMON_H__
