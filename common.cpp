#include "common.hpp"

const int start_event_t::type = 1;
const int stop_event_t::type = 2;

#include <iostream>
#include <thread>
#include <future>
#include <utility>
#include <chrono>

// simple pipeline

simple_event_processor_pipeline_t::simple_event_processor_pipeline_t( event_callback_t && callback )
	: event_processor_pipeline_t( std::forward<event_callback_t>(callback) )
{}

void simple_event_processor_pipeline_t::add_stage( event_processor_sptr_t const& event_processor ){
  if( event_processor && state_ == state_t::idle )
    stages_.push_back( event_processor );
}

event_sptr_t simple_event_processor_pipeline_t::operator()( event_sptr_t const& event ){
  event_sptr_t out_event;

  switch( get_state() ){
    case state_t::idle:
      if(event->get_type() == start_event_t::type)
        set_state( state_t::running );
    break;
    
    case state_t::running:
      if(event->get_type() != stop_event_t::type){
        out_event = event;
        for( auto & stage : stages_ )
          out_event = (*stage)( out_event );
				if( callback_ )
					callback_( out_event );
      }else
        set_state( state_t::stopped );
    break;

    case state_t::stopped: break;
  }

  return out_event;
}

//tbb pipeline

#include <tbb/pipeline.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/tbb_allocator.h>
#include <tbb/concurrent_queue.h>

std::chrono::milliseconds millisec( 1 );
std::chrono::nanoseconds nanosec(1);

class tbb_event_processor_pipeline_t::impl_t{
public:
  //because tbb pipeline operates using raw pointers...
  struct tbb_token_t{ 
    tbb_token_t( event_sptr_t const& e ) : event{e}, done{false} {}
	
    event_sptr_t event;
    bool done;
  };

  class input_filter_t: public tbb::filter {
  public:
    input_filter_t( tbb_event_processor_pipeline_t::impl_t *host ) 
      : tbb::filter(serial_in_order), host_{host}
    //: tbb::filter(parallel), host_{host}
    {}

  private:
    tbb_event_processor_pipeline_t::impl_t *host_;

    void* operator()(void *in){
			//get a token..
			tbb_token_t *token; 
			//read the current event...
			host_->input_.pop( token );
			//pass it further into the chain...
			if( token->event->get_type() != stop_event_t::type )
				return (void*)(token);
			//or stop processing once we receive the stop event...
			token->done = true;
      return nullptr;
    }
  };

  class output_filter_t: public tbb::filter {
  public:
    output_filter_t(tbb_event_processor_pipeline_t::impl_t *host) 
			: tbb::filter(serial_in_order), host_{host}
    //: tbb::filter(parallel), host_{host}
    {}

  private:
    tbb_event_processor_pipeline_t::impl_t *host_;

    void* operator()(void *in){
      if(in){
        auto token = (tbb_token_t*)in;
        token->done = true;
      }

      return nullptr;
    }
  };

  class processing_filter_t: public tbb::filter {
  public:
    processing_filter_t(event_processor_sptr_t const& event_processor) 
      : tbb::filter(parallel), guest_{event_processor}
    {}

  private:
    event_processor_sptr_t guest_;

    void* operator()(void *in){
      if(in){
        //process event
        auto token = (tbb_token_t*)in;
        token->event = (*guest_)(token->event);
        return in;
      }

      return nullptr;
    }
  };

  impl_t( tbb_event_processor_pipeline_t *host, size_t live_tokens ) 
    : host_{host}, live_tokens_{ live_tokens == 0 ? 1 : live_tokens }{
    //add input filter
    stages_.push_back( std::make_shared<input_filter_t>( this ) );
    pipeline_.add_filter( *stages_.back() );
  }

  ~impl_t(){
    //wait for the pipeline to finish processing...
    if( pipeline_thread_.joinable() )
      pipeline_thread_.join();

    //clear pipeline
    pipeline_.clear();
  }

  void add_stage( event_processor_sptr_t const& event_processor ){
    if( event_processor && host_->get_state() == state_t::idle ){
      //add processing filter
      stages_.push_back( std::make_shared<processing_filter_t>( event_processor ) );
      pipeline_.add_filter( *stages_.back() );
    }
  }

  event_sptr_t operator()( event_sptr_t const& event ){
    event_sptr_t comp_event;

    if( event ){
      switch( host_->get_state() ){
        case state_t::idle:
          if( event->get_type() == start_event_t::type ){
            host_->set_state( state_t::running );
            //add the output filter
            stages_.push_back( std::make_shared<output_filter_t>(this) );
            pipeline_.add_filter( *stages_.back() );
            //start pipeline...
            pipeline_thread_ = std::thread( [this](){ pipeline_.run( live_tokens_ ); } );
          }
        break;

        case state_t::running:{
          if( event->get_type() == stop_event_t::type )
            host_->set_state( state_t::stopped );

          //process event
          tbb_token_t token{event};
          input_.push( &token );
          while( !token.done ){ 
            std::this_thread::yield();
            //std::this_thread::sleep_for( millisec );
            //std::this_thread::sleep_for( nanosec );
          }

          if( event->get_type() != stop_event_t::type )
            comp_event = token.event;
        }break;

        case state_t::stopped: break;
      }
    }

    return comp_event;
  }
  
  tbb_event_processor_pipeline_t *host_;

  std::vector<std::shared_ptr<tbb::filter>> stages_;

  tbb::pipeline pipeline_;
  std::thread pipeline_thread_;
  size_t live_tokens_;

  tbb::concurrent_bounded_queue< tbb_token_t* > input_;
};

tbb_event_processor_pipeline_t::tbb_event_processor_pipeline_t( event_callback_t && callback, size_t live_tokens ) 
  : event_processor_pipeline_t( std::forward<event_callback_t>(callback) ), 
    impl_{ new tbb_event_processor_pipeline_t::impl_t{ this, live_tokens } }
{}

tbb_event_processor_pipeline_t::~tbb_event_processor_pipeline_t(){}

void tbb_event_processor_pipeline_t::add_stage( event_processor_sptr_t const& event_processor ){
  impl_->add_stage( event_processor );
}

event_sptr_t tbb_event_processor_pipeline_t::operator()( event_sptr_t const& event ){
  return (*impl_)( event );
}
