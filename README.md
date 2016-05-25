# Elastic Routing

![Language](https://img.shields.io/badge/language-C-lightgrey.svg?style=flat)
[![License](https://img.shields.io/badge/license-GPL-brightgreen.svg?style=flat)](http://mit-license.org)
[![Twitter](https://img.shields.io/badge/twitter-mrbeetle-blue.svg?style=flat)](http://twitter.com/mrbeetle)
![Status](https://img.shields.io/badge/status-under%20development-orange.svg?style=flat)

``elastic routing`` — flexible and powerful vehicle routing solution for the real world written in C.

**WARNING**: this project is not reach the minimal functional point, some key designs are in progress.

## Overview

- Provides a generic model of vehicle routing problem (VRP) which
    - describes various condition and constraints of vehicle routing problem from the practical point of view
    - accepts input of transportation request in a natural and dynamic way
    - changes model state with the execution of the routing plan
    - provides APIs based on which specific sub-models and various heuristics can explored
- Provides an evolution framework which can
    - implement flexible ongoing optimization procedure which adjusts to the dynamic conditions (eg. new request, advance of the plan) and produce feasible and optimized solutions
    - accept multile and customized objectives
- Provides concise yet powerful public APIs to conmunicate with the model and the solver.

# Generic Model (not done)

The generic model describes and represents the largest set of the problem it can be solved. It provides a common I/O interface (API) to the outside caller. It aims to accept input and generate output solutions in a natural and dynamic way. It can of course represent a subset which corresponds to a specified sub-model (eg. CVRP, VRPTW, etc.). In the algorithm level, specific efficient algorithms can be developped for these sub-models.

## Fleet

- Heterogeneous vehicles: vehicles can have different capacities (maximal capacity).
- Each vehicle can be specified a start point which can be any node (not necessary a depot).
- Each vehicle can be specified an end point which can be any node (usually a depot). If the vechicle has no associated request, it would run towards the end point. End point is not required.
- Actual capacity, start point, and end point can be changed with the execution of the plan.

## Transportation Requests

- A request is one of
    - two nodes requeset, which is a pickup-delivery node pair (together with quantity, service time windows, service durations)
    - single node request, which is visiting of a single node (together with quantity, service time window, service duration)
- Request is non-split, which means one request can only be accomplished by one vehicle. This requires that the quantity is smaller than the largest capacity of vehicles.
- However, different requests associated with one node can be accomplished by different vehicles. (depot is a trival example, as it is the pickup node for many requests.)
- Request can be
    - added
    - modified or canceled before execution
    - unchangeable during execution
    - accomplished (archived)

## Roadgraph

- Node type: depot or customer. This attribute is constant. In fact, depots and customers are equal in the view of the request. However, the distinguishment is useful or necessary for some algorithms.
- Multi-depot is supported

## Plan

Feasible solutions ready to be executed to accomplish the requests in the current state of the model. (see below for the concept of state.) So the plan is time dependent, it may varies with the variation of the model, as well as with the continuing optimization.

A plan includes a number of routes. Each route is assigned to a vehicle. Each route specifies a sequence of nodes, along which the vehicle should visit one by one. At each node, it must know which service to give, which requires that the request should be provided with the node. (remember that one node may be associated with multiple requests.)

## State

The concept of state is introduced to resolve a dynamic problem into discrete updatings of static problems. Every state represents a problem setting under a set of known conditions. Events will change these conditions, while state is subjectively changed by the algorithm if it thinks the new events deserved to be considered. The events are such as

- new request added
- request modified or canceled before execution
- advance of plan execution (all or part of a request is executed): the executed part should be archived which should not affect the ongoing optimization
- roadgraph changed: traffic condition changed the distance or duration of arcs
- etc.

## Submodel Integration


# Evolutionary Framework (done)

## Population groups and individual flow

![population groups and individual flow](https://raw.githubusercontent.com/herrkaefer/elastic-routing/master/doc/evol-population.png)

(More explanations will be added later.)

# Distributed Solver (not done)


# Fundamental Data Structures

The following data structures are developped for the project, but they themselves can be used for generic purpose.

## list4u

list container of usigned integers.

## list4x

a generic list container.

## arrayset

set with a built-in hash table.

## queue

simple queue container.

## hash

hash table container.

## matrix4d

a double type matrix.

## rng

random number generator. A wrapper of [PCG](http://www.pcg-random.org/) RNGs.

## timer

timer.

# Installation and Usage


# Todos

## large ones

- VRP base model and sub-models
- Asynchronization

## to fix or test

- arrayset_add(): add a param replacing or not
- add a ref_cnt to arrayset,  delete it when ref_cnt decreases to 0.

## general ideas

- optional type of c
- memory db in c
