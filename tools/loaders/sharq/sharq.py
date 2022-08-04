#!/usr/bin/env python3
"""sharq.py 
This script runs sharq application, captures its telemetry and logs it into NCBI applog 
Telemetry file can be passed directly as sharq's '-t' parameter
or it will be generated automatically by this script

Automatically generated file names is sharq's '--output' parameter + '.rpt' or defualt.sra.out.rpt'

The script does not remove sharq's generated telemetry file

"""

import json
import sys
import subprocess
import os
import time
import datetime
import urllib.parse
import socket
import tempfile
import glob
from pathlib import Path



class Applog:
    """
    Applog class is used to parse sharq telemetry and save it in NCBI applog

    Attributes
    ----------
    applog_cmd : str
        a path to NCBI applog binary
    appname : str
        appname - applog's appname (default sharq)
    token : str
        applog token 
    report_file : str
        name of the sarq's telemetry file 
    start_time: float
        sharq's start time
    """    

    applog_cmd = '/opt/ncbi/bin/ncbi_applog-1'
    appname = 'sharq'
    token = None
    report_file = None
    start_time = None

    def __init__(self, appname, report_file):
        """
        Parameters
        ----------
        appname : str
            applog's appname 
        report_file : str
            sharq's telemetry file 
        """        

        self.appname = appname
        self.report_file = report_file
        self.log_file = tempfile.gettempdir() + "/tmp_" + appname + "." + socket.gethostname() + "." + str(os.getpid()) + "."  + str(int(datetime.datetime.now().timestamp()))

    def start(self):
        """registers app's start and captures the start time"""

        if len(self.log_file) == 0 or len(self.appname) == 0:
            print('Applog was not constructed properly', file=sys.stderr)
            return 
        # Setting NCBI_CONFIG__LOG__FILE re-direct ncbi_applog out  
        # This setting will generate 3 files, the one with '.log' extnsion has raw applog data
        # raw data are then sent to applog by stop() method
        # NCBI_CONFIG__LOG__FILE shoudl be reset before calling ncbi_applog raw

        # NCBI_APPLOG_SITE must be equal to appname in order to be visible in applog 
        os.environ["NCBI_CONFIG__LOG__FILE"] = self.log_file
        os.environ['NCBI_APPLOG_SITE'] = self.appname
        cmd = [self.applog_cmd, 'start_app', '-appname', self.appname, '-pid', str(os.getpid()) ]
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        self.token = proc.stdout.read().decode().rstrip()
        self.start_time = time.time()

    def stop(self, status):
        """
        registers app's stop

        Parameters
        ----------
        status : int
            sharq' return code
        """        

        if len(self.token) == 0:
            print('Applog stop invoked without token', file=sys.stderr)
            return 
        if self.report_file: 
            self.process_report()
        timespan = time.time() - self.start_time
        cmd = [self.applog_cmd, 'stop_app', self.token, '-status', str(status), 
            '-timestamp', str(int(datetime.datetime.now().timestamp())),
            '-exectime',  str(timespan)]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stderr, file=sys.stderr)
        # reset log file redirect    
        del os.environ['NCBI_CONFIG__LOG__FILE']
        r = subprocess.run([self.applog_cmd, 'raw', '-file', self.log_file + '.log'], capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stderr, file=sys.stderr)
        # reset NCBI_APPLOG_SITE
        del os.environ['NCBI_APPLOG_SITE']

        # cleanup log files    
        if self.log_file and 'tmp_' in self.log_file:
            for filePath in glob.glob(self.log_file + '*'):
                try:
                    os.remove(filePath)
                except:
                    print(f"Error while deleting {filePath}", file=sys.stderr)

    def extra(self, params):
        """
        registers applog extra event 

        Parameters
        ----------
        params : str
            event's parameters
        """        

        if len(self.token) == 0:
            print('Applog extra invoked without token', file=sys.stderr)
            return 
        if not params:
            return
        cmd = [self.applog_cmd, 'extra', self.token, '-param', params, 
            '-timestamp', str(int(datetime.datetime.now().timestamp()))]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stderr, file=sys.stderr)


    def post(self, severity, message):
        """
        post applog message 

        Parameters
        ----------
        severity : str
            message severity (warning, error)
        message : str
            message text   
        """        

        if len(self.token) == 0:
            print('Applog post invoked without token', file=sys.stderr)
            return 
        if not message:
            return
        cmd = [self.applog_cmd, 'post', self.token, '-severity', severity, '-message', message,
            '-timestamp', str(int(datetime.datetime.now().timestamp()))]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stderr, file=sys.stderr)

    def process_report(self):
        """  parses sharq' telemetry file and registers corresponding applog events """

        f = open(self.report_file)
        data = json.load(f)
        self.extra(self.json2param(data))
        if 'groups' in data:
            for group in data['groups']:
                self.extra(self.json2param(group))
        if 'error' in data:
            self.post('error', data['error'])

    def json2param(self, js):
        """  converts json objects to url parames
        Parameters
        ----------
        js : object 
            json object 

        Returns
        ----------
        str : formed url parameters
        """            
        p = []
        for i in js:
            s = str(i)
            if s == 'groups' or s == 'error':
                continue
            s += '=' 
            val = ' '.join(js[i]) if isinstance(js[i], list) else str(js[i])
            s += urllib.parse.quote_plus(val)
            p.append(s)
        return '&'.join(p) if len(p) > 0 else ''


def run_sharq():
    """  Runs sharq application 
        the application is expected to be in the same directory as the python script 

        Returns
        ----------
        int : sharq return code
    """
    dir = os.path.dirname(sys.argv[0])
    cmd = [dir + "/sharq"] 
    cmd += sys.argv[1:]
    result = subprocess.run(cmd)
    return result.returncode


def create_applog(appname):
    """  Creates Applog class 
        if script has '-t' or '--telemetry' paramter the function uses it as report_file
        otherwise it modifies sharq's parameter to include telemetry ('-t')
        telemetry files is sharq's output parameter + '.rpt' or default 'sra.out.rpt'

        Parameters
        ----------
        appname : str
            applog's appname

        Returns
        ----------
        object : Applog instance 
    """
    args = sys.argv[1:]
    output = 'sra.out'
    for i, val in enumerate(sys.argv):
        if (val == '-t' or val=='--telemetry') and i + 1 < len(sys.argv):
            return Applog(appname, sys.argv[i + 1])
        elif val == "--output":
            output = sys.argv[i + 1]
    sys.argv.append('-t')
    output += '.rpt'
    sys.argv.append(output)
    return Applog(appname, output)

if __name__=='__main__':

    if len(sys.argv) == 1 or (len(sys.argv)==2 and sys.argv[1]=='--help'):
        #help(__name__)
        print(__doc__)
        sys.exit(0)

    applog = create_applog('sharq')
    applog.start()
    returncode = run_sharq()
    applog.stop(returncode)

    sys.exit(returncode)
