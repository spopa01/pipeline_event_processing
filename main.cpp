#include <iostream>
#include <chrono>
#include <algorithm>
#include <numeric>

#include "pipeline.hpp"

typedef std::chrono::high_resolution_clock hrc;
typedef std::chrono::high_resolution_clock::time_point tp;
typedef std::chrono::milliseconds millisec;

millisec elapsed( tp const& start, tp const& stop ){ 
  return std::chrono::duration_cast<millisec>(stop - start);
}

#define EVENTS_TEST( RESULTS, PIPELINE_TYPE, ... ){                                                               \
  auto start = hrc::now();                                                                                        \
  {                                                                                                               \
    /*CREATE PIPELINE*/                                                                                           \
    auto pipeline = std::make_shared<PIPELINE_TYPE>( [&RESULTS]( event_sptr_t const& event ) -> event_sptr_t {    \
        if( event ) RESULTS.push_back( ( std::static_pointer_cast<value_event_t>(event) )->value_ );              \
        return event;                                                                                             \
      }, ##__VA_ARGS__ );                                                                                         \
    /*ADD STAGES TO THE PIPELINE*/                                                                                \
    ADD_STAGES;                                                                                                   \
    /*START PIPELINE*/                                                                                            \
    (*pipeline)( std::make_shared<start_event_t>() );                                                             \
    /*PROCESS USING PIPELINE*/                                                                                    \
    PROCESS_EVENTS;                                                                                               \
    /*STOP PIPELINES*/                                                                                            \
    (*pipeline)( std::make_shared<stop_event_t>() );                                                              \
  }                                                                                                               \
  auto stop = hrc::now();                                                                                         \
  std::cout << "TIME: " << elapsed(start, stop).count() << std::endl;                                             \
}

//--

//DEFINE SOME CUSTOM EVENT
class value_event_t : public event_t {
public:
  value_event_t( int value ) : value_{value} {}

  static int type() { return 100; }
  int get_type() const override { return type(); }

  int value_;
};

void run_small_events_test(){
  auto num_live_tokens = 8;
  auto num_samples = 1000000;
  
  //DEFINE THE STAGES
  auto ev_proc_1 =
    []( event_sptr_t const& event ) -> event_sptr_t {
      if( event && event->get_type() == value_event_t::type() ){
        return std::make_shared< value_event_t >( ( std::static_pointer_cast<value_event_t>(event) )->value_ * 2 ); //op: x*2
      }
      return event_sptr_t();
    };
  
  auto ev_proc_2 =
    []( event_sptr_t const& event ) -> event_sptr_t {
      if( event && event->get_type() == value_event_t::type() ){
        return std::make_shared< value_event_t >( ( std::static_pointer_cast<value_event_t>(event) )->value_ + 1 ); // op: x+1
      }
      return event_sptr_t();
    };

  //COMPUTE EXPECTED VALUES
  std::vector<int> expected_values(num_samples), serial_pipeline_computed_values, tbb_pipeline_computed_values;
  for( int i=0; i<num_samples; ++i )
    expected_values[i] = i*2+1; // lambda2( lambda1( x ) ) = 2*x + 1
  
  #define ADD_STAGES                  \
  pipeline->add_stage( ev_proc_1 );   \
  pipeline->add_stage( ev_proc_2 );

  #define PROCESS_EVENTS              \
  for( int i=0; i<num_samples; ++i )  \
    (*pipeline)( std::make_shared<value_event_t>( i ) );

  std::cout << "\nMANY_SMALL_EVENTS_TEST ... STARTED" << std::endl;
  
  //RUN SERIAL PIPELINE TEST
  std::cout << "SERIAL_PIPELINE - ";
  EVENTS_TEST( serial_pipeline_computed_values, serial_event_processor_pipeline_t )

  //RUN TBB PIPLINE TEST
  std::cout << "TBB_PIPELINE - ";
  EVENTS_TEST( tbb_pipeline_computed_values, tbb_event_processor_pipeline_t, num_live_tokens )

  #undef PROCESS_EVENTS
  #undef ADD_STAGES

  //CHECK THE RESULTS
  auto isOk = (expected_values == serial_pipeline_computed_values && expected_values == tbb_pipeline_computed_values);
  std::cout << "MANY_SMALL_EVENTS_TEST ... " << ( isOk ? "PASSED" : "FAILED" ) << "\n\n";
}

//--

//DEFINE SOME CUSTOM EVENT
class buffer_event_t : public event_t {
public:
  buffer_event_t( int size, int *values ) : size_{size}, values_{values} {}

  static const int type() { return 200; }
  int get_type() const override { return type(); }

  int size_;
  int *values_;
};

void run_big_events_test(){
  auto num_live_tokens = 8;
  auto num_events = 100;
  auto num_samples_per_event = 10000000;
  
  //DEFINE THE STAGES
  auto ev_proc =
    []( event_sptr_t const& event ) -> event_sptr_t {
      if( event && event->get_type() == buffer_event_t::type() ){
        auto ev = std::static_pointer_cast<buffer_event_t>(event);
        return std::make_shared< value_event_t >( std::inner_product( ev->values_, ev->values_ + ev->size_, ev->values_, 0 ) );
      }
      return event_sptr_t();
    };
  
  std::vector<int> input_values( num_events*num_samples_per_event );
  int* input_buffer = input_values.data();

  std::vector<int> expected_values, serial_pipeline_computed_values, tbb_pipeline_computed_values;
  for( int i=0; i<num_events; ++i ){
    auto curr = input_buffer + i*num_samples_per_event;
    std::fill( curr, curr+num_samples_per_event, i );
    expected_values.push_back( std::inner_product( curr, curr+num_samples_per_event, curr, 0 ) );
  }
  
  #define ADD_STAGES                  \
  pipeline->add_stage( ev_proc );     \
  
  #define PROCESS_EVENTS              \
  for( int i=0; i<num_events; ++i )   \
    (*pipeline)( std::make_shared<buffer_event_t>(num_samples_per_event, input_buffer+i*num_samples_per_event));

  std::cout << "\nFEW_BIG_EVENTS_TEST ... STARTED" << std::endl;
  
  //RUN SERIAL PIPELINE TEST
  std::cout << "SERIAL_PIPELINE - ";
  EVENTS_TEST( serial_pipeline_computed_values, serial_event_processor_pipeline_t )

  //RUN TBB PIPLINE TEST
  std::cout << "TBB_PIPELINE - ";
  EVENTS_TEST( tbb_pipeline_computed_values, tbb_event_processor_pipeline_t, num_live_tokens )
  
  #undef PROCESS_EVENTS
  #undef ADD_STAGES

  //CHECK THE RESULTS
  auto isOk = (expected_values == serial_pipeline_computed_values && expected_values == tbb_pipeline_computed_values);
  std::cout << "FEW_BIG_EVENTS_TEST ... " << ( isOk ? "PASSED" : "FAILED" ) << "\n\n";
}

/* LET'S MOVE SOME DATA AROUND... */

int main(/*...*/){
  run_small_events_test();
  run_big_events_test();
  return 0;
}
