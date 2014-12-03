#include <iostream>

#include "common.hpp"

//DEFINE SOME CUSTOM EVENT
class value_event_t : public event_t {
public:
  value_event_t( int value ) : value_{value} {}

  static const int type = 100;
  int get_type() override { return type; }

  int value_;
};

std::vector<int> run_test( event_processor_pipeline_sptr_t const& pipeline, int num_samples ){
  std::vector<int> computed_values; //move semantics...

  for( int i=0; i<num_samples; ++i ){
    auto event = (*pipeline)( std::make_shared<value_event_t>( i ) );
    if( event )
      computed_values.push_back( ( std::static_pointer_cast<value_event_t>(event) )->value_ );
  }

  return computed_values;
}

int main(/*...*/){
  auto num_live_tokens = 4;
  auto num_samples = 10000;
  
  //DEFINE THE STAGES
  auto ev_proc_lambda1 = std::make_shared< lambda_event_processor_t >(
    []( event_sptr_t const& event ) -> event_sptr_t {
      
      if( event && event->get_type() ){
        auto value = ( std::static_pointer_cast<value_event_t>(event) )->value_ * 2; // op  x*2
        return std::make_shared< value_event_t >( value );
      }

      return event_sptr_t();
    }
  );
  
  auto ev_proc_lambda2 = std::make_shared< lambda_event_processor_t >(
    []( event_sptr_t const& event ) -> event_sptr_t {
      
      if( event && event->get_type() ){
        auto value = ( std::static_pointer_cast<value_event_t>(event) )->value_ + 1; // op : x+1
        return std::make_shared< value_event_t >( value );
      }

      return event_sptr_t();
    }
  );

  //COMPUTE EXPECTED VALUES
  std::vector<int> expected_values;
  for( int i=0; i<num_samples; ++i )
    expected_values.push_back( i*2+1 ); // result = lambda2( lambda1( x ) ) = 2*x + 1

  //CREATE PIPELINES
  auto simple_pipeline = std::make_shared<simple_event_processor_pipeline_t>();
  auto tbb_pipeline = std::make_shared<tbb_event_processor_pipeline_t>( num_live_tokens );

  //ADD STAGES TO THE PIPELINES
  simple_pipeline->add_stage( ev_proc_lambda1 );
  simple_pipeline->add_stage( ev_proc_lambda2 );

  tbb_pipeline->add_stage( ev_proc_lambda1 );
  tbb_pipeline->add_stage( ev_proc_lambda2 );

  //START PIPELINES
  (*simple_pipeline)( std::make_shared<start_event_t>() );
  (*tbb_pipeline)( std::make_shared<start_event_t>() );

  //PROCESS USING PIPELINES
  auto simple_computed_values = run_test( simple_pipeline, num_samples );
  auto tbb_computed_values = run_test( tbb_pipeline, num_samples );
  
  //STOP PIPELINES
  (*simple_pipeline)( std::make_shared<stop_event_t>() );
  (*tbb_pipeline)( std::make_shared<stop_event_t>() );

  //CHECK THE RESULTS
  std::cout << "TEST ... " << ( expected_values == simple_computed_values && expected_values == tbb_computed_values ? "PASSED" : "FAILED" ) << std::endl;

  return 0;
}
