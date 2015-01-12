pipeline_event_processing
=========================

Small framework for writing modular algorithms as programmable pipelines...

Pipelining is a common parallel pattern which simulates a traditional manufacturing assembly line.

The main features are:
 - for each event entering the pipeline, the transformations (aka stages) will be applied in the order they have been defined;
 - all events will leave the pipeline in the same order they entered;
 - TBB pipeline may allow multiple events to be "in flight” in the same time;

Usage:

```C++

#include <iostream>

#include "pipeline.hpp"

//...

/*DEFINE SOME CUSTOM EVENTS*/
class value_event_t : public event_t {
public:
  value_event_t( int value ) : value_{value} {}

  static int type() { return 100; }
  int get_type() const override { return type(); }

  int value_;
};

//...

/*CREATE PIPELINE*/
auto pipeline =
  std::make_shared<tbb_event_processor_pipeline_t>(
    []( event_sptr_t const& event ) -> event_sptr_t { /*the callback*/
      if( event )
        std::cout << ( std::static_pointer_cast<value_event_t>(event) )->value_ << " ";
      else 
        std::cout << "error ";
    return event;
  }, /*active_tokens= */4 );

/*ADD STAGES TO THE PIPELINE*/
pipeline->add_stage( // stage 1
  []( event_sptr_t const& event ) -> event_sptr_t {
    if( event && event->get_type() == value_event_t::type() ){
      return std::make_shared< value_event_t >( ( std::static_pointer_cast<value_event_t>(event) )->value_ * 2 ); //op: x*2
    }
    return event_sptr_t();
  });
pipeline->add_stage( //stage 2
  []( event_sptr_t const& event ) -> event_sptr_t {
    if( event && event->get_type() == value_event_t::type() ){
      return std::make_shared< value_event_t >( ( std::static_pointer_cast<value_event_t>(event) )->value_ + 1 ); // op: x+1
    }
    return event_sptr_t();
  });

/*START PIPELINE*/
(*pipeline)( std::make_shared<start_event_t>() );

/*PROCESS USING PIPELINE*/
for( int i=0; i<10; ++i )
  (*pipeline)( std::make_shared<value_event_t>( i ) );

/*STOP PIPELINES*/
(*pipeline)( std::make_shared<stop_event_t>() ); 

//...

```

Input: 0 1 2 3 4 5 6 7 8 9

// 2 * x + 1

Output: 1 3 5 7 9 11 13 15 17 19 
