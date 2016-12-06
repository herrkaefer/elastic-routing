#!/usr/bin/env python

from pyvrp import VRPSolver
from pprint import pprint



def read_cvrptw_benchmark(filename):
    data = {"num_vehicles": None, "max_capacity": None, "nodes": []}
    current_data = None
    with open(filename) as f:
        for line in f.readlines():
            if line.startswith("NUMBER"):
                current_data = "vehicle"
                continue
            if line.startswith("CUST NO."):
                current_data = "customer"
                continue

            if current_data == "vehicle":
                line = line.split()
                if line:
                    data["num_vehicles"] = int(line[0])
                    data["max_capacity"] = float(line[1])
                current_data = None

            elif current_data == "customer":
                line = line.split()
                if line:
                    data["nodes"].append({
                        "coord": (float(line[1]), float(line[2])),
                        "demand": float(line[3]),
                        "time_window": (int(line[4]), int(line[5])),
                        "service_duration": int(line[6])
                        })
            else:
                continue
    return data


def create_vrptw_solver_from_benchmark(filename):
    data = read_cvrptw_benchmark(filename)
    solver = VRPSolver()

    solver.set_coord_sys('CARTESIAN2D')

    for idx, node in enumerate(data["nodes"]):
        if idx == 0:
            depot = node
            depot_id = solver.add_node (ext_id="node"+str(idx), type='depot')
            solver.set_node_coord (depot_id, node['coord'])
            continue

        nid = solver.add_node (ext_id="node"+str(idx), type='customer')
        solver.set_node_coord (nid, node['coord'])

        # Add requests
        rid = solver.add_request(ext_id='request'+str(idx),
                                 sender=depot_id,
                                 receiver=nid,
                                 quantity=node['demand'])
        solver.add_time_window(rid, 'sender',
                               depot['time_window'][0], depot['time_window'][1])
        solver.add_time_window(rid, "receiver",
                               node['time_window'][0], node['time_window'][1])
        solver.set_service_duration(rid, 'sender', depot['service_duration'])
        solver.set_service_duration(rid, "receiver", node['service_duration'])

    solver.generate_beeline_distances()
    # solver.generate_durations(1.0)

    for idx in range(data['num_vehicles']):
        solver.add_vehicle("vehicle"+str(idx), data['max_capacity'], depot_id, depot_id)

    return solver


if __name__ == '__main__':
    filename = "benchmark/vrptw/solomon_100/R101.txt"
    solver = create_vrptw_solver_from_benchmark(filename)
    print("solver created.")
    solver.solve()



