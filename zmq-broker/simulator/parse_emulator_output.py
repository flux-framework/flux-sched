#!/usr/bin/env python

import argparse
import csv
import re

header = ["jobid", "csvid", "submit", "schedule", "run", "complete"]

class Job ():

    def __init__ (self, jobid, csvid, submit=None, schedule=None, run=None, complete=None):
        self.jobid = jobid
        self.csvid = csvid
        self.submit_time = submit
        self.schedule_time = schedule
        self.run_time = run
        self.complete_time = complete

    def set_value (self, key, value):
        if key == 'submit':
            self.set_submit (value)
        elif key == 'schedule':
            self.set_schedule (value)
        elif key == 'run':
            self.set_run (value)
        elif key == 'complete':
            self.set_complete (value)

    def set_submit (self, submit):
        if self.submit_time is not None:
            print "Overwriting existing submit time"
        self.submit_time = submit

    def set_schedule (self, schedule):
        if self.schedule_time is not None:
            print "Overwriting existing schedule time"
        self.schedule_time = schedule

    def set_run (self, run):
        if self.run_time is not None:
            print "Overwriting existing run time"
        self.run_time = run

    def set_complete (self, complete):
        if self.complete_time is not None:
            print "Overwriting existing complete time"
        self.complete_time = complete

    def to_list (self):
        return [self.jobid, self.csvid, self.submit_time, self.schedule_time, self.run_time, self.complete_time]

def generate_events (filename):
    time_regex = re.compile ("Triggering.*Curr sim time: ([0-9\.]+)")
    submit_regex = re.compile ("submitted job ([0-9]+) \(([0-9]+) in csv\)")
    schedule_regex = re.compile ("scheduled job ([0-9]+)")
    run_regex = re.compile ("job ([0-9]+)'s state to starting then running")
    complete_regex = re.compile ("Job ([0-9]+) completed")
    curr_time = 0
    with open (filename, 'r') as infile:
        for row in infile:
            match = time_regex.search (row)
            if match:
                curr_time = float(match.group (1))
            else:
                match = submit_regex.search (row)
                if match:
                    yield ('submit', (int (match.group (1)), int (match.group (2))), curr_time)
                else:
                    match = schedule_regex.search (row)
                    if match:
                        yield ('schedule', int (match.group (1)), curr_time)
                    else:
                        match = run_regex.search (row)
                        if match:
                            yield ('run', int (match.group (1)), curr_time)
                        else:
                            match = complete_regex.search (row)
                            if match:
                                yield ('complete', int (match.group (1)), curr_time)

def parse_file (events):
    jobs = {}
    for event_type, value, curr_time in events:
        if event_type == 'submit':
            jobid = value[0]
            csvid = value[1]
            jobs[jobid] = Job (jobid, csvid)
        else:
            jobid = value
        jobs[jobid].set_value (event_type, curr_time)
    return jobs

def save_results (jobs, output_name):
    with open (output_name, 'w') as outfile:
        writer = csv.writer (outfile)
        writer.writerow (header)
        for job in jobs:
            writer.writerow (job.to_list ())

def main ():
    parser = argparse.ArgumentParser ()
    parser.add_argument ("infile")
    parser.add_argument ("outfile")
    args = parser.parse_args ()

    infile = args.infile
    outfile = args.outfile

    events = generate_events (infile)
    jobs_dict = parse_file (events)
    jobs = [jobs_dict[key] for key in jobs_dict]
    save_results (jobs, outfile)

if __name__ == "__main__":
    main ()
