#!/bin/false
#
#  Run script as `flux ion-resource` with properly configured
#   FLUX_EXEC_PATH or `flux python flux-ion-resource` if not to
#   avoid python version mismatch
#
from __future__ import print_function
import argparse
import errno
import yaml
import json
import flux
import time

def heading ():
    return '{:20} {:20} {:20} {:20}'.format ('JOBID', 'STATUS',
                                             'AT', 'OVERHEAD (Secs)')

def body (jobid, status, at, overhead):
    t = time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime (at))
    o = str (overhead)
    return '{:20} {:20} {:20} {:20}'.format (str (jobid), status, t, o)

def width ():
    return 20 + 20 + 20 + 20

"""
    Class to interface with the sched-fluxion-resource module with RPC
"""
class ResourceModuleInterface:
    def __init__ (self):
        self.f = flux.Flux ()

    def rpc_next_jobid (self):
        resp = self.f.rpc ("sched-fluxion-resource.next_jobid").get ()
        return resp['jobid']

    def rpc_allocate (self, jobid, jobspec_str):
        payload = {'cmd' : 'allocate', 'jobid' : jobid, 'jobspec' : jobspec_str}
        return self.f.rpc ("sched-fluxion-resource.match", payload).get ()

    def rpc_update (self, jobid, R):
        payload = {'jobid' : jobid, 'R' : R}
        return self.f.rpc ("sched-fluxion-resource.update", payload).get ()

    def rpc_allocate_with_satisfiability (self, jobid, jobspec_str):
        payload = {'cmd' : 'allocate_with_satisfiability',
                   'jobid' : jobid, 'jobspec' : jobspec_str}
        return self.f.rpc ("sched-fluxion-resource.match", payload).get ()

    def rpc_reserve (self, jobid, jobspec_str):
        payload = {'cmd' : 'allocate_orelse_reserve',
                   'jobid' : jobid, 'jobspec' : jobspec_str}
        return self.f.rpc ("sched-fluxion-resource.match", payload).get ()

    def rpc_info (self, jobid):
        payload = {'jobid' : jobid}
        return self.f.rpc ("sched-fluxion-resource.info", payload).get ()

    def rpc_stat (self):
        return self.f.rpc ("sched-fluxion-resource.stat").get ()

    def rpc_cancel (self, jobid):
        payload = {'jobid' : jobid}
        return self.f.rpc ("sched-fluxion-resource.cancel", payload).get ()
   
    def rpc_set_property (self, sp_resource_path, sp_keyval):
        payload = {'sp_resource_path' : sp_resource_path, 
            'sp_keyval' : sp_keyval}
        return self.f.rpc ("sched-fluxion-resource.set_property", payload).get ()
    
    def rpc_get_property (self, gp_resource_path, gp_key):
        payload = {'gp_resource_path' : gp_resource_path, 'gp_key' : gp_key}
        return self.f.rpc ("sched-fluxion-resource.get_property", payload).get ()

    def rpc_find (self, criteria):
        payload = {'criteria' : criteria}
        return self.f.rpc ("sched-fluxion-resource.find", payload).get ()
    

"""
    Action for match allocate sub-command
"""
def match_alloc_action (args):
    with open (args.jobspec, 'r') as stream:
        jobspec_str = yaml.dump (yaml.safe_load (stream))
        r = ResourceModuleInterface ()
        resp = r.rpc_allocate (r.rpc_next_jobid (), jobspec_str)
        print (heading ())
        print (body (resp['jobid'], resp['status'], resp['at'], resp['overhead']))
        print ("=" * width ())
        print ("MATCHED RESOURCES:")
        print (resp['R'])

"""
    Action for match allocate_with_satisfiability sub-command
"""
def match_alloc_sat_action (args):
    with open (args.jobspec, 'r') as stream:
        jobspec_str = yaml.dump (yaml.safe_load (stream))
        r = ResourceModuleInterface ()
        resp = r.rpc_allocate_with_satisfiability (r.rpc_next_jobid (),
                                                   jobspec_str)
        print (heading ())
        print (body (resp['jobid'], resp['status'], resp['at'], resp['overhead']))
        print ("=" * width ())
        print ("MATCHED RESOURCES:")
        print (resp['R'])

"""
    Action for match allocate_orelse_reserve sub-command
"""
def match_reserve_action (args):
    with open (args.jobspec, 'r') as stream:
        jobspec_str = yaml.dump (yaml.safe_load (stream))
        r = ResourceModuleInterface ()
        resp = r.rpc_reserve (r.rpc_next_jobid (), jobspec_str)
        print (heading ())
        print (body (resp['jobid'], resp['status'], resp['at'], resp['overhead']))
        print ("=" * width ())
        print ("MATCHED RESOURCES:")
        print (resp['R'])

"""
    Action for update sub-command
"""
def update_action (args):
    with open (args.RV1, 'r') as stream:
        RV1 = json.dumps (json.load (stream))
        r = ResourceModuleInterface ()
        resp = r.rpc_update (args.jobid, RV1)
        print (heading ())
        print (body (resp['jobid'], resp['status'], resp['at'], resp['overhead']))
        print ("=" * width ())
        print ("UPDATED RESOURCES:")
        print (resp['R'])

"""
    Action for cancel sub-command
"""
def cancel_action (args):
    r = ResourceModuleInterface ()
    jobid = args.jobid
    resp = r.rpc_cancel (jobid)

"""
    Action for info sub-command
"""
def info_action (args):
    r = ResourceModuleInterface ()
    jobid = args.jobid
    resp = r.rpc_info (jobid)
    print (heading ())
    print (body (resp['jobid'], resp['status'], resp['at'], resp['overhead']))

"""
    Action for stat sub-command
"""
def stat_action (args):
    r = ResourceModuleInterface ()
    resp = r.rpc_stat ()
    print ("Num. of Vertices: ", resp['V'])
    print ("Num. of Edges: ", resp['E'])
    print ("Num. of Vertices by Rank: ", json.dumps (resp['by_rank']))
    print ("Graph Load Time: ", resp['load-time'], "Secs")
    print ("Num. of Jobs Matched: ", resp['njobs'])
    print ("Min. Match Time: ", resp['min-match'], "Secs")
    print ("Max. Match Time: ", resp['max-match'], "Secs")
    print ("Avg. Match Time: ", resp['avg-match'], "Secs")

"""
    Action for set-property sub-command
"""
def set_property_action (args):
    r = ResourceModuleInterface ()
    sp_resource_path = args.sp_resource_path
    sp_keyval = args.sp_keyval
    resp = r.rpc_set_property (sp_resource_path, sp_keyval)

"""
    Action for get-property sub-command
"""
def get_property_action (args):
    r = ResourceModuleInterface ()
    gp_resource_path = args.gp_resource_path
    gp_key = args.gp_key
    resp = r.rpc_get_property (gp_resource_path, gp_key)
    print (args.gp_key, "=", resp['value'])

"""
    Action for find sub-command
"""
def find_action (args):
    r = ResourceModuleInterface ()
    resp = r.rpc_find (args.criteria)
    print ('CRITERIA')
    print ("\'" + args.criteria + "\'")
    print ("=" * width ())
    print ('MATCHED RESOURCES:')
    print (json.dumps (resp['R']))


"""
    Main entry point
"""
def main ():
    #
    # Main command arguments/options
    #
    desc = 'Front-end command for sched-fluxion-resource '\
           'module for testing. Provide 4 sub-commands. '\
           'For sub-command usage, '\
           '%(prog)s <sub-command> --help'
    parser = argparse.ArgumentParser (description=desc)
    parser.add_argument ('-v', '--verbose', action='store_true',
                         help='be verbose')

    #
    # Add subparser for the top-level sub-commands
    #
    subpar = parser.add_subparsers (title='Available Commands',
                                    description='Valid commands',
                                    help='Additional help')
    mstr = "Find the best matching resources for a jobspec"
    ustr = "Update the resource database"
    istr = "Print info on a single job"
    sstr = "Print overall performance statistics"
    cstr = "Cancel an allocated or reserved job"
    fstr = "Find resources matching with a crieria"
    pstr = "Set property-key=value for specified resource."
    gstr = "Get value for specified resource and property-key."
    parser_m = subpar.add_parser ('match', help=mstr, description=mstr)
    parser_u = subpar.add_parser ('update', help=ustr, description=ustr)
    parser_i = subpar.add_parser ('info', help=istr, description=istr)
    parser_s = subpar.add_parser ('stat', help=sstr, description=sstr)
    parser_c = subpar.add_parser ('cancel', help=cstr, description=cstr)
    parser_f = subpar.add_parser ('find', help=fstr, description=fstr)
    parser_sp = subpar.add_parser ('set-property', help=pstr, description=pstr)
    parser_gp = subpar.add_parser ('get-property', help=gstr, description=gstr)

    #
    # Add subparser for the match sub-command
    #
    subparsers_m = parser_m.add_subparsers (title='Available Commands',
                                           description='Valid commands',
                                           help='Additional help')

    mastr = "Allocate the best matching resources if found"
    msstr = "Allocate the best matching resources if found. "\
            "If not found, check jobspec's overall satisfiability"
    mrstr = "Allocate the best matching resources if found. "\
            "If not found, reserve them instead at earliest time"
    parser_ma = subparsers_m.add_parser ('allocate', help=mastr)
    parser_ms = subparsers_m.add_parser ('allocate_with_satisfiability',
                                          help=msstr)
    parser_mr = subparsers_m.add_parser ('allocate_orelse_reserve', help=mrstr)

    #
    # Positional argument for update sub-command
    #
    parser_u.add_argument ('RV1', metavar='RV1', type=str, help='RV1 file name')
    parser_u.add_argument ('jobid', metavar='Jobid', type=int, help='Jobid')
    parser_u.set_defaults (func=update_action)

    #
    # Positional argument for info sub-command
    #
    parser_i.add_argument ('jobid', metavar='Jobid', type=int, help='Jobid')
    parser_i.set_defaults (func=info_action)

    #
    # Action for stat sub-command
    #
    parser_s.set_defaults (func=stat_action)

    #
    # Positional argument for cancel sub-command
    #
    parser_c.add_argument ('jobid', metavar='Jobid', type=int, help='Jobid')
    parser_c.set_defaults (func=cancel_action)

    #
    # Positional argument for find sub-command
    #
    cr_help='Matching criteria -- a compound expression must be quoted'
    parser_f.add_argument ('criteria', metavar='Criteria', type=str, help=cr_help)
    parser_f.set_defaults (func=find_action)

    #
    # Positional argument for match allocate sub-command
    #
    parser_ma.add_argument ('jobspec', metavar='Jobspec', type=str,
                            help='Jobspec file name')
    parser_ma.set_defaults (func=match_alloc_action)

    #
    # Positional argument for match allocate_with_satisfiability sub-command
    #
    parser_ms.add_argument ('jobspec', metavar='Jobspec', type=str,
                            help='Jobspec file name')
    parser_ms.set_defaults (func=match_alloc_sat_action)

    #
    # Positional argument for match allocate_orelse_reserve sub-command
    #
    parser_mr.add_argument ('jobspec', metavar='Jobspec', type=str,
                            help='Jobspec file name')
    parser_mr.set_defaults (func=match_reserve_action)

    # Positional arguments for set-property sub-command
    #
    parser_sp.add_argument ('sp_resource_path', metavar='ResourcePath', 
            type=str, help='set-property resource_path property-key=val')
    parser_sp.add_argument ('sp_keyval', metavar='PropertyKeyVal', type=str,
                            help='set-property resource_path property-key=val')
    parser_sp.set_defaults (func=set_property_action)

    # Positional argument for get-property sub-command
    #
    parser_gp.add_argument ('gp_resource_path', metavar='ResourcePath', 
                type=str, help='get-property resource_path property-key')
    parser_gp.add_argument ('gp_key', metavar='PropertyKey', type=str,
                            help='get-property resource_path property-key')
    parser_gp.set_defaults (func=get_property_action)

    #
    # Parse the args and call an action routine as part of that
    #
    try:
        args = parser.parse_args ()
        args.func (args)

    except (IOError,EnvironmentError) as e:
        name = e.__class__.__name__
        print ("{}: error({}): {}".format (name, e.errno, e.strerror))
        if e.errno == errno.ENOENT:
            exit (3)
        if e.errno == errno.EBUSY:  # resource currently unavailable
            exit (16)
        if e.errno == errno.ENODEV: # unsatisfiable jobspec
            exit (19)
        else:
            exit (1)

    except yaml.YAMLError as e:
        print ("Parsing error: ", e)
        exit (2)

if __name__ == "__main__":
    main ()

#
# vi:tabstop=4 shiftwidth=4 expandtab
#
