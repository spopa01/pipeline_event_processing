#include "common.hpp"

#include <iostream>

const int start_event_t::type = 1;
const int stop_event_t::type = 2;

//TBB Pipeline

#include <tbb/pipeline.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/tbb_allocator.h>
#include <tbb/concurrent_queue.h>

#include <queue>
#include <thread>

class tbb_event_processor_pipeline_t::impl_t{
public:
  //because tbb pipeline operates using raw pointers...
  struct tbb_token_t{ event_sptr_t event; };

  class input_filter_t: public tbb::filter {
  public:
    input_filter_t( tbb_event_processor_pipeline_t::impl_t *host ) 
      : tbb::filter(serial_in_order), host_{host}
    {}

  private:
    tbb_event_processor_pipeline_t::impl_t *host_;

    void* operator()(void *in){
      if( host_->host_->get_state() == state_t::running ){
        //get a token
        auto token = host_->tokens_.front();
        host_->tokens_.pop();
        //read current event
        host_->input_.pop( token->event );
        //pass it further in the chain...
        if( !token->event->get_type() != stop_event_t::type )
          return (void*)(token);
        else{
          host_->tokens_.push( token );
        }
      }

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
        auto token = (tbb_token_t*)in;
        //push event to the output...
        host_->output_.push( token->event );
        //recycle received token...
        host_->tokens_.push( token );
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
        auto token = (tbb_token_t*)in;
        //process event
        token->event = (*guest_)(token->event);
        return in;
      }

      return nullptr;
    }
  };

  impl_t( tbb_event_processor_pipeline_t *host, size_t live_tokens ) 
    : host_{host}, live_tokens_{ live_tokens == 0 ? 1 : live_tokens }{
    //input_.set_capacity( 1000 );

    //allocate tokens
    for( auto i=live_tokens; i>0; --i )
      tokens_.push( tbb::tbb_allocator<tbb_token_t>().allocate(1) );
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

    //deallocate tokens
    for( int i=live_tokens_; i>0; --i ){
      auto token = tokens_.front();
      tokens_.pop();
      tbb::tbb_allocator<tbb_token_t>().deallocate( token, 1 );
    }
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

    switch( host_->get_state() ){
      case state_t::idle:
        if( event && event->get_type() == start_event_t::type ){
          host_->set_state( state_t::running );
          //add the output filter
          stages_.push_back( std::make_shared<output_filter_t>(this) );
          pipeline_.add_filter( *stages_.back() );
          //start pipeline...
          pipeline_thread_ = std::thread( [this](){ pipeline_.run( live_tokens_ ); } );
        }
      break;

      case state_t::running:
        if( event&& event->get_type() != stop_event_t::type ){
          //process event
          input_.push( event );
          output_.pop( comp_event );
        }else{
          //signal stop pipeline...
          host_->set_state( state_t::stopped );
          input_.push( event );
        }
      break;

      case state_t::stopped: break;
    }

    return comp_event;
  }
  
  tbb_event_processor_pipeline_t *host_;

  std::vector<std::shared_ptr<tbb::filter>> stages_;

  tbb::pipeline pipeline_;
  std::thread pipeline_thread_;

  std::queue<tbb_token_t*> tokens_;
  size_t live_tokens_;

  tbb::concurrent_bounded_queue< event_sptr_t > input_;
  tbb::concurrent_bounded_queue< event_sptr_t > output_;
};

tbb_event_processor_pipeline_t::tbb_event_processor_pipeline_t( size_t live_tokens ) 
  : event_processor_pipeline_t(), 
    impl_{ new tbb_event_processor_pipeline_t::impl_t{ this, live_tokens } }
{}

tbb_event_processor_pipeline_t::~tbb_event_processor_pipeline_t(){}

void tbb_event_processor_pipeline_t::add_stage( event_processor_sptr_t const& event_processor ){
  impl_->add_stage( event_processor );
}

event_sptr_t tbb_event_processor_pipeline_t::operator()( event_sptr_t const& event ){
  return (*impl_)( event );
}
