#include "common.hpp"

#include <iostream>
#include <thread>
#include <future>
#include <utility>
#include <chrono>

// simple pipeline

simple_event_processor_pipeline_t::simple_event_processor_pipeline_t( event_processor_func_t && callback )
	: event_processor_pipeline_t( std::forward<event_processor_func_t>(callback) )
{}

void simple_event_processor_pipeline_t::add_stage( event_processor_func_t && processor ){
	if( processor && get_state() == state_t::idle )
		stages_.push_back( processor );
}

void simple_event_processor_pipeline_t::operator()( event_sptr_t const& event ){
	event_sptr_t out_event;

	switch( get_state() ){
		case state_t::idle:
			if(event->get_type() == start_event_t::type())
				set_state( state_t::running );
		break;

		case state_t::running:
			if(event->get_type() != stop_event_t::type()){
				out_event = event;
				for( auto & stage : stages_ )
					out_event = stage( out_event );
			}else
				set_state( state_t::stopped );
		break;

		case state_t::stopped: break;
	}

	if( callback_ )
		callback_( out_event );
}

//tbb pipeline

#include <tbb/pipeline.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/tbb_allocator.h>
#include <tbb/concurrent_queue.h>

class tbb_event_processor_pipeline_t::impl_t{
public:
  //because tbb pipeline operates using raw pointers...
  struct tbb_token_t{ 
		event_sptr_t event;
  };

  class input_filter_t: public tbb::filter {
  public:
    input_filter_t( tbb_event_processor_pipeline_t::impl_t *host ) 
      : tbb::filter(serial_in_order), host_{host}
    {}

  private:
    tbb_event_processor_pipeline_t::impl_t *host_;

    void* operator()(void *in){
			//get a token..
			tbb_token_t *token;
			host_->tokens_.pop( token );
			//read the current event...
			host_->events_.pop( token->event );
			//pass it further into the pipeline...
			if( token->event->get_type() != stop_event_t::type() )
				return (void*)(token);
			//or stop processing once we received a stop event...
			//also recycle the current token...
			host_->tokens_.push( token );
      return nullptr;
    }
  };

	class output_filter_t: public tbb::filter {
	public:
		output_filter_t(tbb_event_processor_pipeline_t::impl_t *host) 
			: tbb::filter(serial_in_order), host_{host}
		{}

  private:
    tbb_event_processor_pipeline_t::impl_t *host_;

    void* operator()(void *in){
      if(in){
				//send back the result...
				auto token = (tbb_token_t*)in;
				if( host_->host_->callback_ )
					host_->host_->callback_(token->event);
				//recycle the current token...
				host_->tokens_.push( token );
      }

      return nullptr;
    }
  };

  class processing_filter_t: public tbb::filter {
  public:
		processing_filter_t(event_processor_func_t && processor) 
			: tbb::filter(parallel), 
				processor_{std::forward<event_processor_func_t>(processor)}
		{}

  private:
    event_processor_func_t processor_;

    void* operator()(void *in){
      if(in){
        //process the current event...
        auto token = (tbb_token_t*)in;
        token->event = processor_(token->event);
        return in;
      }

      return nullptr;
    }
  };

	impl_t( tbb_event_processor_pipeline_t *host, size_t live_tokens ) 
		: host_{host}, live_tokens_{ live_tokens == 0 ? 1 : live_tokens }
	{
		for( auto i = 0; i <= live_tokens_; ++i )
			tokens_.push( new tbb_token_t );

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

		//release the cache of tokens...
		for( auto i = 0; i <= live_tokens_; ++i ){
			tbb_token_t *token;
			tokens_.pop( token );
			delete token;
		}
  }

  void add_stage( event_processor_func_t && processor ){
    if( processor && host_->get_state() == state_t::idle ){
      //add processing filter
      stages_.push_back( std::make_shared<processing_filter_t>( std::forward<event_processor_func_t>(processor) ) );
      pipeline_.add_filter( *stages_.back() );
    }
  }

  void operator()( event_sptr_t const& event ){
    if( event ){
      switch( host_->get_state() ){
        case state_t::idle:
          if( event->get_type() == start_event_t::type() ){
            host_->set_state( state_t::running );
            //add the output filter
            stages_.push_back( std::make_shared<output_filter_t>(this) );
            pipeline_.add_filter( *stages_.back() );
            //start pipeline...
            pipeline_thread_ = std::thread( [this](){ pipeline_.run( live_tokens_ ); } );
          }
        break;

        case state_t::running:{
          if( event->get_type() == stop_event_t::type() )
            host_->set_state( state_t::stopped );
          //enqueue the current event...
          events_.push( event );
        }break;

        case state_t::stopped: break;
      }
    }
  }
  
	tbb_event_processor_pipeline_t *host_;

	std::vector<std::shared_ptr<tbb::filter>> stages_;

	tbb::pipeline pipeline_;
	std::thread pipeline_thread_;

	size_t live_tokens_;
	tbb::concurrent_bounded_queue< tbb_token_t* > tokens_;

	tbb::concurrent_bounded_queue< event_sptr_t > events_;
};

tbb_event_processor_pipeline_t::tbb_event_processor_pipeline_t( event_processor_func_t && callback, size_t live_tokens ) 
  : event_processor_pipeline_t( std::forward<event_processor_func_t>(callback) ), 
    impl_{ std::make_unique<tbb_event_processor_pipeline_t::impl_t>( this, live_tokens ) }
{}

tbb_event_processor_pipeline_t::~tbb_event_processor_pipeline_t(){}

void tbb_event_processor_pipeline_t::add_stage( event_processor_func_t && processor ){
	impl_->add_stage( std::forward<event_processor_func_t>(processor) );
}

void tbb_event_processor_pipeline_t::operator()( event_sptr_t const& event ){
	(*impl_)( event );
}
