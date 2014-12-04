#include <iostream>
#include <chrono>
#include <algorithm>
#include <numeric>

#include "common.hpp"

typedef std::chrono::high_resolution_clock hrc;
typedef std::chrono::high_resolution_clock::time_point tp;
typedef std::chrono::milliseconds millisec;

millisec elapsed( tp const& start, tp const& stop ){ 
  return std::chrono::duration_cast<millisec>(stop - start);
}

//--

//DEFINE SOME CUSTOM EVENT
class value_event_t : public event_t {
public:
  value_event_t( int value ) : value_{value} {}

  static const int type = 100;
  int get_type() override { return type; }

  int value_;
};

std::vector<int> run_small_events_test( event_processor_pipeline_sptr_t const& pipeline, int num_samples ){
	std::vector<int> computed_values; //move semantics...

	auto start = hrc::now();
  
	for( int i=0; i<num_samples; ++i ){
    auto event = (*pipeline)( std::make_shared<value_event_t>( i ) );
    if( event )
      computed_values.push_back( ( std::static_pointer_cast<value_event_t>(event) )->value_ );
  }
	
	auto stop = hrc::now();
	std::cout << "TIME: " << elapsed(start, stop).count() << std::endl;
	
  return computed_values;
}

void run_small_events_test(){
  auto num_live_tokens = 4;
  auto num_samples = 100000;
  
  //DEFINE THE STAGES
  auto ev_proc_lambda1 = std::make_shared< lambda_event_processor_t >(
    []( event_sptr_t const& event ) -> event_sptr_t {
      if( event && event->get_type() == value_event_t::type ){
        return std::make_shared< value_event_t >( ( std::static_pointer_cast<value_event_t>(event) )->value_ * 2 ); //op: x*2
      }

      return event_sptr_t();
    }
  );
  
  auto ev_proc_lambda2 = std::make_shared< lambda_event_processor_t >(
    []( event_sptr_t const& event ) -> event_sptr_t {
      if( event && event->get_type() == value_event_t::type ){
        return std::make_shared< value_event_t >( ( std::static_pointer_cast<value_event_t>(event) )->value_ + 1 ); // op: x+1
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
  auto simple_computed_values = run_small_events_test( simple_pipeline, num_samples );
  auto tbb_computed_values = run_small_events_test( tbb_pipeline, num_samples );
  
  //STOP PIPELINES
  (*simple_pipeline)( std::make_shared<stop_event_t>() );
  (*tbb_pipeline)( std::make_shared<stop_event_t>() );

  //CHECK THE RESULTS
  std::cout << "SMALL_EVENTS_TEST ... " << ( expected_values == simple_computed_values && expected_values == tbb_computed_values ? "PASSED" : "FAILED" ) << std::endl;
}

//--

//DEFINE SOME CUSTOM EVENT
class buffer_event_t : public event_t {
public:
  buffer_event_t( int size, int *values ) : size_{size}, values_{values} {}

  static const int type = 200;
  int get_type() override { return type; }

  int size_;
  int *values_;
};

int run_big_events_test( event_processor_pipeline_sptr_t const& pipeline, int num_events, int num_samples_per_event ){
  std::vector<int> computed_values;

  std::vector<int> values;
  for( int i=0; i<num_events; ++i ){
      for( int j=0; j<num_samples_per_event; ++j )
          values.push_back( i );
  }

  int* buff = values.data();

  auto start = hrc::now();
  
  for( int i=0; i<num_events; ++i ){
    auto event = (*pipeline)( std::make_shared<buffer_event_t>( num_samples_per_event, buff + i * num_samples_per_event ) );
    if( event ){
      computed_values.push_back( ( std::static_pointer_cast<value_event_t>(event) )->value_ );
    }
  }

  auto stop = hrc::now();
  std::cout << "TIME: " << elapsed(start, stop).count() << std::endl;

  return std::accumulate( computed_values.begin(), computed_values.end(), 0 );
}

#include <thread>
#include <functional>
#include <vector>

using task_t = std::function<void()>;

thread_local unsigned int thread_num;
thread_local unsigned int num_threads;

class thread_pool{
private:
  std::vector<std::thread> pool;

public:
  template<typename T>
  thread_pool( size_t n_thrds, T task ){
    for( size_t n = 0; n < n_thrds; ++n )
      pool.emplace_back([=](){ thread_num = n; num_threads = n_thrds; task(); });
  }

  void wait(){ for( auto& thrd : pool ) thrd.join(); }
  void no_wait(){ for( auto& thrd : pool ) thrd.detach(); }
};

#define parallel_do(N, ...) thread_pool (N, [__VA_ARGS__]()
#define end_wait ).wait();
#define end_no_wait ).no_wait();

#define split(cond) if(cond)
#define single(N) if(thread_num == N)
#define master if(thread_num == 0)

#include <tbb/concurrent_queue.h>
int run_big_events_test_mt( event_processor_pipeline_sptr_t const& pipeline, int num_events, int num_samples_per_event ){
  const int NT = 4;
  std::array<std::vector<int>, NT> computed_values;

  int NPT = num_events/NT;

  std::vector<int> values;
  for( int i=0; i<num_events; ++i ){
      for( int j=0; j<num_samples_per_event; ++j )
          values.push_back( i );
  }

  int* buff = values.data();

  auto start = hrc::now();
  
  parallel_do( NT, &pipeline, &computed_values, buff, NPT, num_samples_per_event ){
    int cnt = 0;
    int offset = thread_num * NPT;
    while( cnt++ < NPT ){
      auto event = (*pipeline)( std::make_shared<buffer_event_t>( num_samples_per_event, buff + (offset+cnt) * num_samples_per_event ) );
      if( event ){
        computed_values[thread_num].push_back( ( std::static_pointer_cast<value_event_t>(event) )->value_ );
      }
    }
  }end_wait;

  auto stop = hrc::now();
  std::cout << "TIME: " << elapsed(start, stop).count() << std::endl;

  int value = 0;
  for( auto & arr : computed_values ){
    value = std::accumulate( arr.begin(), arr.end(), value );
  }

  return value;
}

void run_big_events_test(){
  auto num_live_tokens = 4;
  auto num_events = 100;
  auto num_samples_per_event = 1000000;
  
  //DEFINE THE STAGES
  auto ev_proc_lambda = std::make_shared< lambda_event_processor_t >(
    []( event_sptr_t const& event ) -> event_sptr_t {
      if( event && event->get_type() == buffer_event_t::type ){
		auto ev = std::static_pointer_cast<buffer_event_t>(event);
		return std::make_shared< value_event_t >( std::inner_product( ev->values_, ev->values_ + ev->size_, ev->values_, 0 ) );
      }

      return event_sptr_t();
    }
  );
 
  //CREATE PIPELINES
  auto simple_pipeline = std::make_shared<simple_event_processor_pipeline_t>();
  auto tbb_pipeline = std::make_shared<tbb_event_processor_pipeline_t>( num_live_tokens );

  //ADD STAGES TO THE PIPELINES
  simple_pipeline->add_stage( ev_proc_lambda );
  tbb_pipeline->add_stage( ev_proc_lambda );

  //START PIPELINES
  (*simple_pipeline)( std::make_shared<start_event_t>() );
  (*tbb_pipeline)( std::make_shared<start_event_t>() );
  
  //PROCESS USING PIPELINES
  auto simple_computed_value = run_big_events_test( simple_pipeline, num_events, num_samples_per_event );
  auto tbb_computed_value = run_big_events_test( tbb_pipeline, num_events, num_samples_per_event );
  
  //STOP PIPELINES
  (*simple_pipeline)( std::make_shared<stop_event_t>() );
  (*tbb_pipeline)( std::make_shared<stop_event_t>() );

  //CHECK THE RESULTS
  std::cout << "BIG_EVENTS_TEST ... " << ( simple_computed_value == tbb_computed_value ? "PASSED" : "FAILED" ) << std::endl;
}

int main(/*...*/){
  run_small_events_test();
  run_big_events_test();
  return 0;
}
