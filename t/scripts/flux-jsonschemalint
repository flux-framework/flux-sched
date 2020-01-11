#!/usr/bin/env python

from __future__ import print_function

import argparse
import errno
import json
import jsonschema

"""
    Print json document and schema as a verbose option
"""
def print_verbose (doc, schema):
    print("==============================================")
    print(" Jsonschema ")
    print("==============================================")
    print(json.dumps (schema, indent=4, sort_keys=True))
    print("")
    print("==============================================")
    print(" Json document ")
    print("==============================================")
    print(json.dumps (doc, indent=4, sort_keys=True))
    print("")


"""
    Main entry point
"""
def main ():
    desc = 'Validate an JSON document against an JSON scheme.'
    parser = argparse.ArgumentParser (description=desc)
    parser.add_argument ('schema', help='JSON schema file name')
    parser.add_argument ('json', help='JSON document file name')
    parser.add_argument ('-v', '--verbose', action='store_true',
                         help='be verbose')
    try:
        doc = ""
        schema = ""
        args = parser.parse_args ()
        with open (args.schema) as f:
            schema = json.load (f)
        with open (args.json, 'r') as f2:
            doc = json.load (f2)
        if args.verbose:
            print_verbose (doc, schema)

        jsonschema.validate (doc, schema)
        valstr = "Document(\"{0}\") validates against schema (\"{1}\")"
        print (valstr.format (args.json, args.schema))

    except IOError as e:
        print ("I/O error({0}): {1}".format (e.errno, e.strerror))
        exit (1)
    except EnvironmentError as e:
        print ("Environment error({0}): {1}".format (e.errno, e.strerror))
        exit (1)
    except ValueError as estr:
        print ("Value error: {0}".format (str (estr)))
        exit (1)
    except jsonschema.exceptions.ValidationError as e:
        print ("Jsonschema validation error: {0}".format (e.message))
        exit (1)
    except jsonschema.exceptions.SchemaError as e:
        print ("Jsonschema schema error: {0}".format (e.message))
        exit (1)

if __name__ == "__main__":
    main ()
#
# vi:tabstop=4 shiftwidth=4 expandtab
#
