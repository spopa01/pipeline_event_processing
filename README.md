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

#define SCAST_VAL_T( EV ) std::static_pointer_cast<value_event_t>(EV)->value_
#define IS_VAL_T( EV ) EV && EV->get_type() == value_event_t::type()

/* CREATE A (NOT SO USEFUL) PIPELINE TO COMPUTE 2x+1 */
auto pipeline =
  std::make_shared<tbb_event_processor_pipeline_t>(
    []( event_sptr_t const& event ) -> event_sptr_t { /*the callback*/
      std::cout << (IS_VAL_T(event) ? std::to_string(SCAST_VAL_T(event)) : std::string("error")) << " ";
      return event;
    }, /*active_tokens= */4 );

/* ADD STAGES TO THE PIPELINE */
pipeline->add_stage( // stage 1 : y = 2*x
  []( event_sptr_t const& event ) -> event_sptr_t {
    if( IS_VAL_T(event) ){ SCAST_VAL_T(event) *= 2; }
    return event;
  });
pipeline->add_stage( //stage 2 : z = y+1 = 2x+1
  []( event_sptr_t const& event ) -> event_sptr_t {
    if( IS_VAL_T(event) ){ SCAST_VAL_T(event) += 1; }
    return event;
  });

/* START PIPELINE */
(*pipeline)( std::make_shared<start_event_t>() );

/* PROCESS USING THE PIPELINE */
for( int i=0; i<10; ++i )
  (*pipeline)( std::make_shared<value_event_t>( i ) );

/* STOP PIPELINES */
(*pipeline)( std::make_shared<stop_event_t>() ); 

//...

```

Input(x): 0 1 2 3 4 5 6 7 8 9

Output(2x+1): 1 3 5 7 9 11 13 15 17 19 
