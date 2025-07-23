#!/bin/false
#
#  Run script as `flux ion-resource` with properly configured
#   FLUX_EXEC_PATH or `flux python flux-ion-resource` if not to
#   avoid python version mismatch
#
import argparse
import errno
import json
import sys
import time

import flux
import yaml

from flux.job import JobID


def heading():
    return "{:20} {:20} {:20} {:20}".format("JOBID", "STATUS", "AT", "OVERHEAD (Secs)")


def body(jobid, status, at_time, overhead):
    timeval = time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(at_time))
    overhead = str(overhead)
    return "{:20} {:20} {:20} {:20}".format(str(jobid), status, timeval, overhead)


def width():
    return 20 + 20 + 20 + 20


class ResourceModuleInterface:
    """Class to interface with the sched-fluxion-resource module with RPC"""

    def __init__(self):
        self.handle = flux.Flux()

    def rpc_next_jobid(self):
        resp = self.handle.rpc("sched-fluxion-resource.next_jobid").get()
        return resp["jobid"]

    def rpc_allocate(self, jobid, jobspec_str):
        payload = {"cmd": "allocate", "jobid": jobid, "jobspec": jobspec_str}
        return self.handle.rpc("sched-fluxion-resource.match", payload).get()

    def rpc_match_grow(self, jobid, jobspec_str):
        payload = {"cmd": "grow_allocation", "jobid": jobid, "jobspec": jobspec_str}
        return self.handle.rpc("sched-fluxion-resource.match", payload).get()

    def rpc_update(self, jobid, Res):
        payload = {"jobid": jobid, "R": Res}
        return self.handle.rpc("sched-fluxion-resource.update", payload).get()

    def rpc_allocate_with_sat(self, jobid, jobspec_str):
        payload = {
            "cmd": "allocate_with_satisfiability",
            "jobid": jobid,
            "jobspec": jobspec_str,
        }
        return self.handle.rpc("sched-fluxion-resource.match", payload).get()

    def rpc_reserve(self, jobid, jobspec_str):
        payload = {
            "cmd": "allocate_orelse_reserve",
            "jobid": jobid,
            "jobspec": jobspec_str,
        }
        return self.handle.rpc("sched-fluxion-resource.match", payload).get()

    def rpc_info(self, jobid):
        payload = {"jobid": jobid}
        return self.handle.rpc("sched-fluxion-resource.info", payload).get()

    def rpc_stat(self):
        return self.handle.rpc("sched-fluxion-resource.stats-get").get()

    def rpc_stats_clear(self):
        return self.handle.rpc("sched-fluxion-resource.stats-clear").get()

    def rpc_cancel(self, jobid):
        payload = {"jobid": jobid}
        return self.handle.rpc("sched-fluxion-resource.cancel", payload).get()

    def rpc_partial_cancel(self, jobid, rv1exec):
        payload = {"jobid": jobid, "R": rv1exec}
        return self.handle.rpc("sched-fluxion-resource.partial-cancel", payload).get()

    def rpc_set_property(self, sp_resource_path, sp_keyval):
        payload = {"sp_resource_path": sp_resource_path, "sp_keyval": sp_keyval}
        return self.handle.rpc("sched-fluxion-resource.set_property", payload).get()

    def rpc_remove_property(self, rp_resource_path, rp_key):
        payload = {"resource_path": rp_resource_path, "key": rp_key}
        return self.handle.rpc("sched-fluxion-resource.remove_property", payload).get()

    def rpc_get_property(self, gp_resource_path, gp_key):
        payload = {"gp_resource_path": gp_resource_path, "gp_key": gp_key}
        return self.handle.rpc("sched-fluxion-resource.get_property", payload).get()

    def rpc_find(self, criteria, find_format=None):
        payload = {"criteria": criteria}
        if find_format:
            payload["format"] = find_format
        return self.handle.rpc("sched-fluxion-resource.find", payload).get()

    def rpc_status(self):
        return self.handle.rpc("sched-fluxion-resource.status").get()

    def rpc_set_status(self, resource_path, status):
        payload = {"resource_path": resource_path, "status": status}
        return self.handle.rpc("sched-fluxion-resource.set_status", payload).get()

    def rpc_namespace_info(self, rank, type_name, identity):
        payload = {"rank": rank, "type-name": type_name, "id": identity}
        return self.handle.rpc("sched-fluxion-resource.ns-info", payload).get()

    def rpc_satisfiability(self, jobspec):
        payload = {"jobspec": jobspec}
        return self.handle.rpc("sched-fluxion-resource.satisfiability", payload).get()

    def rpc_params(self):
        return self.handle.rpc("sched-fluxion-resource.params").get()


def match_alloc_action(args):
    """
    Action for match allocate sub-command
    """

    with open(args.jobspec, "r") as stream:
        jobspec_str = yaml.dump(yaml.safe_load(stream))
        rmormod = ResourceModuleInterface()
        resp = rmormod.rpc_allocate(rmormod.rpc_next_jobid(), jobspec_str)
        print(heading())
        print(body(resp["jobid"], resp["status"], resp["at"], resp["overhead"]))
        print("=" * width())
        print("MATCHED RESOURCES:")
        print(resp["R"])


def match_grow_action(args):
    """
    Action for match grow sub-command
    """

    with open(args.jobspec, "r") as stream:
        jobspec_str = yaml.dump(yaml.safe_load(stream))
        jobid = args.jobid
        rmormod = ResourceModuleInterface()
        resp = rmormod.rpc_match_grow(jobid, jobspec_str)
        print(heading())
        print(body(resp["jobid"], resp["status"], resp["at"], resp["overhead"]))
        print("=" * width())
        print("MATCHED RESOURCES:")
        print(resp["R"])


def match_alloc_sat_action(args):
    """
    Action for match allocate_with_satisfiability sub-command
    """

    with open(args.jobspec, "r") as stream:
        jobspec_str = yaml.dump(yaml.safe_load(stream))
        rmod = ResourceModuleInterface()
        resp = rmod.rpc_allocate_with_sat(rmod.rpc_next_jobid(), jobspec_str)
        print(heading())
        print(body(resp["jobid"], resp["status"], resp["at"], resp["overhead"]))
        print("=" * width())
        print("MATCHED RESOURCES:")
        print(resp["R"])


def match_reserve_action(args):
    """
    Action for match allocate_orelse_reserve sub-command
    """

    with open(args.jobspec, "r") as stream:
        jobspec_str = yaml.dump(yaml.safe_load(stream))
        rmod = ResourceModuleInterface()
        resp = rmod.rpc_reserve(rmod.rpc_next_jobid(), jobspec_str)
        print(heading())
        print(body(resp["jobid"], resp["status"], resp["at"], resp["overhead"]))
        print("=" * width())
        print("MATCHED RESOURCES:")
        print(resp["R"])


def satisfiability_action(args):
    """
    Action for match satisfiability sub-command
    """

    with open(args.jobspec, "r") as stream:
        jobspec = yaml.safe_load(stream)
        rmod = ResourceModuleInterface()
        rmod.rpc_satisfiability(jobspec)
        print("=" * width())
        print("Satisfiable request")
        print("=" * width())


def update_action(args):
    """
    Action for update sub-command
    """

    with open(args.RV1, "r") as stream:
        RV1 = json.dumps(json.load(stream))
        rmod = ResourceModuleInterface()
        resp = rmod.rpc_update(args.jobid, RV1)
        print(heading())
        print(body(resp["jobid"], resp["status"], resp["at"], resp["overhead"]))
        print("=" * width())
        print("UPDATED RESOURCES:")
        print(resp["R"])


def cancel_action(args):
    """
    Action for cancel sub-command
    """

    rmod = ResourceModuleInterface()
    jobid = args.jobid
    rmod.rpc_cancel(jobid)


def partial_cancel_action(args):
    """
    Action for partial cancel sub-command
    """

    with open(args.rv1exec, "r") as stream:
        rv1exec = json.dumps(json.load(stream))
        rmod = ResourceModuleInterface()
        jobid = args.jobid
        rmod.rpc_partial_cancel(jobid, rv1exec)


def info_action(args):
    """
    Action for info sub-command
    """

    rmod = ResourceModuleInterface()
    jobid = args.jobid
    resp = rmod.rpc_info(jobid)
    print(heading())
    print(body(resp["jobid"], resp["status"], resp["at"], resp["overhead"]))


def stat_action(_):
    """
    Action for stat sub-command
    """

    rmod = ResourceModuleInterface()
    resp = rmod.rpc_stat()
    print("Num. of Vertices: ", resp["V"])
    print("Num. of Edges: ", resp["E"])
    print("Num. of Vertices by Rank: ", json.dumps(resp["by_rank"]))
    print("Graph Load Time: ", resp["load-time"], "Secs")
    print("Graph Upime: ", resp["graph-uptime"], "Secs")
    print("Time Since Stats Reset: ", resp["time-since-reset"], "Secs")
    print(
        "Num. of Total Jobs Successfully Matched: ",
        resp["match"]["succeeded"]["njobs"],
    )
    print(
        "Num. of Jobs Successfully Matched Since Reset: ",
        resp["match"]["succeeded"]["njobs-reset"],
    )
    print(
        "Jobid corresponding to max match time: ",
        resp["match"]["succeeded"]["max-match-jobid"],
    )
    print(
        "Match iterations corresponding to max match time: ",
        resp["match"]["succeeded"]["max-match-iters"],
    )
    print(
        "Min. Successful Match Time: ",
        resp["match"]["succeeded"]["stats"]["min"],
        "Secs",
    )
    print(
        "Max. Successful Match Time: ",
        resp["match"]["succeeded"]["stats"]["max"],
        "Secs",
    )
    print(
        "Avg. Successful Match Time: ",
        resp["match"]["succeeded"]["stats"]["avg"],
        "Secs",
    )
    print(
        "Successful Match Variance: ",
        resp["match"]["succeeded"]["stats"]["variance"],
        "Secs^2",
    )
    print(
        "Num. of Jobs with Failed Matches: ",
        resp["match"]["failed"]["njobs"],
    )
    print(
        "Num. of Jobs with Failed Matches Since Reset: ",
        resp["match"]["failed"]["njobs-reset"],
    )
    print(
        "Jobid corresponding to max match time: ",
        resp["match"]["failed"]["max-match-jobid"],
    )
    print(
        "Match iterations corresponding to max match time: ",
        resp["match"]["failed"]["max-match-iters"],
    )
    print(
        "Min. Match Time of Failed Matches: ",
        resp["match"]["failed"]["stats"]["min"],
        "Secs",
    )
    print(
        "Max. Match Time of Failed Matches: ",
        resp["match"]["failed"]["stats"]["max"],
        "Secs",
    )
    print(
        "Avg. Match Time of Failed Matches: ",
        resp["match"]["failed"]["stats"]["avg"],
        "Secs",
    )
    print(
        "Match Variance of Failed Matches: ",
        resp["match"]["failed"]["stats"]["variance"],
        "Secs^2",
    )


def stats_clear_action(_):
    """
    Action for stats clear sub-command
    """

    rmod = ResourceModuleInterface()
    rmod.rpc_stats_clear()


def set_property_action(args):
    """
    Action for set-property sub-command
    """

    rmod = ResourceModuleInterface()
    sp_resource_path = args.sp_resource_path
    sp_keyval = args.sp_keyval
    rmod.rpc_set_property(sp_resource_path, sp_keyval)


def remove_property_action(args):
    """
    Action for remove-property sub-command
    """
    rmod = ResourceModuleInterface()
    rmod.rpc_remove_property(args.rp_resource_path, args.rp_key)


def get_property_action(args):
    """
    Action for get-property sub-command
    """

    rmod = ResourceModuleInterface()
    gp_resource_path = args.gp_resource_path
    gp_key = args.gp_key
    resp = rmod.rpc_get_property(gp_resource_path, gp_key)
    print(args.gp_key, "=", resp["values"])


def find_action(args):
    """
    Action for find sub-command
    """

    rmod = ResourceModuleInterface()
    resp = rmod.rpc_find(args.criteria, find_format=args.format)
    if not args.quiet:
        print("CRITERIA")
        print("'" + args.criteria + "'")
        print("=" * width())
        print("MATCHED RESOURCES:")
    print(json.dumps(resp["R"]))


def status_action(_):
    """
    Action for status sub-command
    """

    rmod = ResourceModuleInterface()
    resp = rmod.rpc_status()
    print("CRITERIA")
    print("all: 'status=up or status=down'")
    print("down: 'status=down'")
    print("allocated: 'sched-now=allocated'")
    print("=" * width())
    print("MATCHED RESOURCES:")
    print(json.dumps(resp))


def set_status_action(args):
    """
    Action for set-status sub-command
    """

    rmod = ResourceModuleInterface()
    rmod.rpc_set_status(args.resource_path, args.status)


def namespace_info_action(args):
    """
    Action for ns-info sub-command
    """

    rmod = ResourceModuleInterface()
    resp = rmod.rpc_namespace_info(args.rank, args.type, args.Id)
    print(resp["id"])


def params_action(_):
    """
    Action for params sub-command
    """

    rmod = ResourceModuleInterface()
    resp = rmod.rpc_params()
    print(json.dumps(resp))


def parse_match(parser_m: argparse.ArgumentParser):
    """
    Add subparser for the match sub-command
    """

    subparsers_m = parser_m.add_subparsers(
        title="Available Commands", description="Valid commands", help="Additional help"
    )

    parser_ma = subparsers_m.add_parser(
        "allocate", help="Allocate the best matching resources if found."
    )
    parser_mg = subparsers_m.add_parser(
        "grow_allocation",
        help="Grow allocation with the best matching resources if found.",
    )
    parser_ms = subparsers_m.add_parser(
        "allocate_with_satisfiability",
        help=(
            "Allocate the best matching resources if found. "
            "If not found, check jobspec's overall satisfiability."
        ),
    )
    parser_mr = subparsers_m.add_parser(
        "allocate_orelse_reserve",
        help=(
            "Allocate the best matching resources if found. "
            "If not found, reserve them instead at earliest time."
        ),
    )
    parser_fe = subparsers_m.add_parser(
        "satisfiability", help="Check jobspec's overall satisfiability."
    )

    #
    # Jobspec positional argument for all match sub-commands
    #
    for subparser in parser_ma, parser_mg, parser_ms, parser_mr, parser_fe:
        subparser.add_argument(
            "jobspec", metavar="Jobspec", type=str, help="Jobspec file name"
        )
    # Add jobid positional argument for match grow
    parser_mg.add_argument("jobid", metavar="Jobid", type=JobID, help="Jobid")

    parser_ma.set_defaults(func=match_alloc_action)
    parser_mg.set_defaults(func=match_grow_action)
    parser_ms.set_defaults(func=match_alloc_sat_action)
    parser_mr.set_defaults(func=match_reserve_action)
    parser_fe.set_defaults(func=satisfiability_action)


def parse_update(parser_u: argparse.ArgumentParser):
    #
    # Positional argument for update sub-command
    #
    parser_u.add_argument("RV1", metavar="RV1", type=str, help="RV1 file name")
    parser_u.add_argument("jobid", metavar="Jobid", type=JobID, help="Jobid")
    parser_u.set_defaults(func=update_action)


def parse_info(parser_i: argparse.ArgumentParser):
    #
    # Positional argument for info sub-command
    #
    parser_i.add_argument("jobid", metavar="Jobid", type=JobID, help="Jobid")
    parser_i.set_defaults(func=info_action)


def parse_find(parser_f: argparse.ArgumentParser):
    #
    # Positional argument for find sub-command
    #
    parser_f.add_argument(
        "criteria",
        metavar="Criteria",
        type=str,
        help="Matching criteria -- a compound expression must be quoted",
    )
    parser_f.add_argument(
        "--format", type=str, default=None, help="Writer format for find query"
    )
    parser_f.add_argument("-q", "--quiet", action="store_true", help="be quiet")
    parser_f.set_defaults(func=find_action)


def parse_set_status(parser_ss: argparse.ArgumentParser):
    # Positional argument for set-status sub-command
    #
    parser_ss.add_argument(
        "resource_path",
        help="path to vertex",
    )
    parser_ss.add_argument(
        "status",
        help="status of vertex",
    )
    parser_ss.set_defaults(func=set_status_action)


# pylint: disable=too-many-statements
def main():
    """
    Main entry point
    """
    #
    # Main command arguments/options
    #
    parser = argparse.ArgumentParser(
        description=(
            "Front-end command for sched-fluxion-resource "
            "module for testing. Provide 4 sub-commands. "
            "For sub-command usage, "
            "%(prog)s <sub-command> --help"
        )
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="be verbose")

    #
    # Add subparser for the top-level sub-commands
    #
    subpar = parser.add_subparsers(
        title="Available Commands", description="Valid commands", help="Additional help"
    )

    def mkparser(name, help_desc):
        return subpar.add_parser(name, help=help_desc, description=help_desc)

    parse_match(mkparser("match", "Find the best matching resources for a jobspec."))
    parse_update(mkparser("update", "Update the resource database."))
    parse_info(mkparser("info", "Print info on a single job."))
    parser_s = mkparser("stats", "Print overall performance statistics.")
    parser_sc = mkparser("stats-cancel", "Clear overall performance statistics.")
    parser_c = mkparser("cancel", "Cancel an allocated or reserved job.")
    parser_pc = mkparser("partial-cancel", "Partially cancel an allocated job.")
    parse_find(mkparser("find", "Find resources matching with a criteria."))
    parser_st = mkparser("status", "Display resource status.")
    parse_set_status(mkparser("set-status", "Set up/down status of a resource vertex."))
    parser_sp = mkparser(
        "set-property", "Set property-key=value for specified resource."
    )
    parser_rp = mkparser("remove-property", "Remove property for specified resource.")
    parser_gp = mkparser(
        "get-property", "Get value for specified resource and property-key."
    )
    parser_n = mkparser("ns-info", "Get remapped ID given raw ID seen by the reader.")
    parser_pa = mkparser("params", "Display the module's parameter values.")

    #
    # Action for stat sub-command
    #
    parser_s.set_defaults(func=stat_action)

    #
    # Action for stats-clear sub-command
    #
    parser_sc.set_defaults(func=stats_clear_action)

    #
    # Positional argument for cancel sub-command
    #
    parser_c.add_argument("jobid", metavar="Jobid", type=JobID, help="Jobid")
    parser_c.set_defaults(func=cancel_action)

    #
    # Positional argument for partial cancel sub-command
    #
    parser_pc.add_argument("jobid", metavar="Jobid", type=JobID, help="Jobid")
    parser_pc.add_argument("rv1exec", metavar="rv1exec", type=str, help="RV1exec")
    parser_pc.set_defaults(func=partial_cancel_action)

    #
    # Positional argument for find sub-command
    #
    parser_st.set_defaults(func=status_action)

    # Positional arguments for set-property sub-command
    #
    parser_sp.add_argument(
        "sp_resource_path",
        metavar="ResourcePath",
        type=str,
        help="set-property resource_path property-key=val",
    )
    parser_sp.add_argument(
        "sp_keyval",
        metavar="PropertyKeyVal",
        type=str,
        help="set-property resource_path property-key=val",
    )
    parser_sp.set_defaults(func=set_property_action)

    # Positional arguments for remove-property sub-command
    #
    parser_rp.add_argument(
        "rp_resource_path",
        metavar="ResourcePath",
        type=str,
        help="remove-property resource_path property-key=val",
    )
    parser_rp.add_argument(
        "rp_key",
        metavar="PropertyKey",
        type=str,
        help="remove-property resource_path property-key",
    )
    parser_rp.set_defaults(func=remove_property_action)

    # Positional argument for get-property sub-command
    #
    parser_gp.add_argument(
        "gp_resource_path",
        metavar="ResourcePath",
        type=str,
        help="get-property resource_path property-key",
    )
    parser_gp.add_argument(
        "gp_key",
        metavar="PropertyKey",
        type=str,
        help="get-property resource_path property-key",
    )
    parser_gp.set_defaults(func=get_property_action)

    # Positional argument for ns-info sub-command
    #
    parser_n.add_argument("rank", metavar="Rank", type=int, help="execution target")
    parser_n.add_argument(
        "type", metavar="Type", type=str, help="type: e.g., core, gpu, or rank"
    )
    parser_n.add_argument(
        "Id", metavar="Id", type=int, help="raw Id seen by the reader"
    )
    parser_n.set_defaults(func=namespace_info_action)

    #
    # Positional argument for params sub-command
    #
    parser_pa.set_defaults(func=params_action)

    #
    # Parse the args and call an action routine as part of that
    #
    try:
        args = parser.parse_args()

        # A subcommand is required
        if not hasattr(args, "func"):
            parser.print_help()
            print("\nA subcommand is required.")
            sys.exit(1)

        args.func(args)

    except (IOError, EnvironmentError) as exc:
        name = exc.__class__.__name__
        print("{}: error({}): {}".format(name, exc.errno, exc.strerror))
        if exc.errno == errno.ENOENT:
            sys.exit(3)
        if exc.errno == errno.EBUSY:  # resource currently unavailable
            sys.exit(16)
        if exc.errno == errno.ENODEV:  # unsatisfiable jobspec
            sys.exit(19)
        else:
            sys.exit(1)

    except yaml.YAMLError as exc:
        print("Parsing error: ", exc)
        sys.exit(2)


if __name__ == "__main__":
    main()

#
# vi:tabstop=4 shiftwidth=4 expandtab
#
