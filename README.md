# Elastic Routing

``elastic routing`` — practical and flexible vehicle routing solution.

## Goal

- Provides a generic model of vehicle routing problem (VRP) which
    - describes various condition and constraints of vehicle routing problem from the practical point of view
    - accepts input of transportation request in a natural and dynamic way
    - changes model state with the execution of the routing plan
    - provides APIs based on which specific sub-models and various heuristics can explored
- Provides an evolution framework which can
    - implement flexible ongoing optimization procedure which adjusts to the dynamic conditions (eg. new request, advance of the plan) and produce feasible and optimized solutions
    - accept multile and customized objectives
- Provides concise yet powerful public APIs to conmunicate with the model and the solver.

## Generic Model

The generic model describes and represents the largest set of the problem it can be solved. It provides a common I/O interface (API) to the outside caller. It aims to accept input and generate output solutions in a natural and dynamic way. It can of course represent a subset which corresponds to a specified sub-model (eg. CVRP, VRPTW, etc.). In the algorithm level, specific efficient algorithms can be developped for these sub-models.

### Fleet

- Heterogeneous vehicles: vehicles can have different capacities (maximal capacity).
- Each vehicle can be specified a start point which can be any node (not necessary a depot).
- Each vehicle can be specified an end point which can be any node (usually a depot). If the vechicle has no associated request, it would run towards the end point. End point is not required.
- Actual capacity, start point, and end point can be changed with the execution of the plan.

### Transportation Requests

- A request is one of
    - a pickup-delivery node pair (together with quantity, service time windows, service durations)
    - visiting of a node (together with service time window, service duration)
- Request is non-split. One request can only be accomplished by one vehicle. This requires that the quantity is smaller than the largest capacity of vehicles.
- Requests associated with one node can be accomplished by different vehicles. (depot is a trival example.)
- Request can be
    - added
    - modified or canceled before execution
    - unchangeable during execution
    - accomplished (archived)

### Roadgraph

- Node type: depot or customer. This attribute is constant. In fact, depots and customers are equal in the view of the request. However, the distinguishment is useful or necessary for some algorithms.
- Multi-depot is supported

### plan

Feasible solutions ready to be executed.

### State

The concept of state is introduced to resolve a dynamic problem into discrete updatings of static problems. Every state represents a problem setting under a set of known conditions. Events will change these conditions, while state is subjectively changed by the algorithm if it thinks the new events deserved to be considered. The events are such as

- new request added
- request modified or canceled before execution
- advance of plan execution (all or part of a request is executed): the executed part should be archived which should not affect the ongoing optimization
- roadgraph changed: traffic condition changed the distance or duration of arcs
- etc.



## Solver framework

Evolutionary framework

# APIs

# Installation

# Todos

## large ones

- VRP base model and sub-models
- Asynchronization

## to fixes or test

- arrayset_add(): add a param replacing or not
- add a ref_cnt to arrayset,  delete it when ref_cnt decreases to 0.

## general ideas

- optional type of c
- memory db in c





