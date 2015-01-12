pipeline_event_processing
=========================

Small framework for writing modular algorithms as programmable pipelines...

Pipelining is a common parallel pattern which simulates a traditional manufacturing assembly line.

It's main features are:
 - for each event entering the pipeline, the transformations (aka stages) will be applied in the order they have been defined;
 - all events will leave the pipeline in the same order they entered;
 - TBB pipeline may allow multiple events to be "in flight” in the same time;
