#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <memory>
#include <functional>
#include <vector>

//EVENT DEFINITION

class event_t{
public:
  event_t(){}
  virtual ~event_t(){}

  virtual int get_type() = 0;
};

using event_sptr_t = std::shared_ptr< event_t >;

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

class event_processor_t {
public:
  event_processor_t(){}
  virtual ~event_processor_t(){}

  virtual event_sptr_t operator()( event_sptr_t const& event ) = 0;
};

using event_processor_sptr_t = std::shared_ptr< event_processor_t >;

class lambda_event_processor_t : public event_processor_t {
public:
  lambda_event_processor_t( std::function<event_sptr_t(event_sptr_t const&)>&& lambda ) 
    : event_processor_t(),
      lambda_{std::forward<std::function<event_sptr_t(event_sptr_t const&)>>(lambda)} 
  {}

  event_sptr_t operator()( event_sptr_t const& event ) override {
    return ( lambda_ ? lambda_( event ) : event_sptr_t() );
  }

private:
  std::function<event_sptr_t(event_sptr_t const&)> lambda_;
};

//EVENT PROCESSOR PIPELINE DEFINITION

//base class
class event_processor_pipeline_t : public event_processor_t {
public:
  event_processor_pipeline_t() : event_processor_t() { state_ = state_t::idle; }
  virtual void add_stage( event_processor_sptr_t const& event_processor ) = 0;

protected:
  enum class state_t : char  { idle, running, stopped };
  state_t state_;
};

using event_processor_pipeline_sptr_t = std::shared_ptr<event_processor_pipeline_t>;

//simple pipeline
class simple_event_processor_pipeline_t : public event_processor_pipeline_t {
public:
  simple_event_processor_pipeline_t() : event_processor_pipeline_t() {}

  void add_stage( event_processor_sptr_t const& event_processor ) override {
    if( event_processor && state_ == state_t::idle )
      stages_.push_back( event_processor );
  }

  event_sptr_t operator()( event_sptr_t const& event ) override {
    event_sptr_t comp_event;

    switch( state_ ){
      case state_t::idle:
        if(event->get_type() == start_event_t::type)
          state_ = state_t::running;
      break;
      
      case state_t::running:
        if(event->get_type() != stop_event_t::type){
          comp_event = event;
          for( auto & stage : stages_ )
            comp_event = (*stage)( comp_event );
        }else
          state_ = state_t::stopped;
      break;

      case state_t::stopped: break;
    }

    return comp_event;
  }

private:
  std::vector< event_processor_sptr_t > stages_;
};

//tbb pipeline
class tbb_event_processor_pipeline_t : public event_processor_pipeline_t {
public:
  tbb_event_processor_pipeline_t( size_t live_tokens );
  virtual ~tbb_event_processor_pipeline_t();

  void add_stage( event_processor_sptr_t const& event_processor ) override;
  event_sptr_t operator()( event_sptr_t const& event ) override;

private:
  class impl_t;
  std::unique_ptr<impl_t> impl_;

  void set_state( state_t state ){ state_ = state; }
  state_t get_state(){ return state_; }
};

#endif//__EVENTS_H__
