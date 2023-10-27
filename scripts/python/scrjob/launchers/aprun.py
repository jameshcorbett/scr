from time import sleep

from scrjob import config
from scrjob.launchers import JobLauncher
from scrjob.common import runproc


class APRUN(JobLauncher):

    def __init__(self, launcher='aprun'):
        super(APRUN, self).__init__(launcher=launcher)

    # a command to run immediately before prerun is ran
    # NOP srun to force every node to run prolog to delete files from cache
    # TODO: remove this if admins find a better place to clear cache
    def prepare_prerun(self):
        # NOP aprun to force every node to run prolog to delete files from cache
        # TODO: remove this if admins find a better place to clear cache
        argv = ['aprun', '/bin/hostname']  # ,'>','/dev/null']
        runproc(argv=argv)

    # returns the process and PID of the launched process
    # as returned by runproc(argv=argv, wait=False)
    def launch_run_cmd(self, up_nodes='', down_nodes='', launcher_args=[]):
        if type(launcher_args) is str:
            launcher_args = launcher_args.split()

        # aprun needs to specify nodes to use
        if len(launcher_args) == 0 or len(up_nodes) == 0:
            return None, -1
        argv = [self.launcher]
        argv.extend(['-L', up_nodes])
        argv.extend(launcher_args)

        ### TODO: #ensure the Popen.terminate() works here too.
        if config.USE_JOBLAUNCHER_KILL != '1':
            return runproc(argv=argv, wait=False)

        proc = runproc(argv=argv, wait=False)[0]
        jobstepid = self.jobstep_id(pid=proc.pid)
        if jobstepid is not None:
            return proc, jobstepid
        else:
            return proc, proc

    # perform a generic pdsh / clustershell command
    # returns [ [ stdout, stderr ] , returncode ]
    def parallel_exec(self, argv=[], runnodes=''):
        if len(argv) == 0:
            return [['', ''], 0]
        if self.clustershell_task != False:
            return self.clustershell_exec(argv=argv, runnodes=runnodes)
        pdshcmd = [
            config.PDSH_EXE, '-Rexec', '-f', '256', '-S', '-w', runnodes
        ]
        pdshcmd.extend(argv)
        return runproc(argv=pdshcmd, getstdout=True, getstderr=True)

    # perform the scavenge files operation
    # uses either pdsh or clustershell
    # returns a list -> [ 'stdout', 'stderr' ]
    def scavenge_files(self,
                       prog='',
                       upnodes='',
                       downnodes_spaced='',
                       cntldir='',
                       dataset_id='',
                       prefixdir='',
                       buf_size='',
                       crc_flag=''):
        argv = [
            'aprun', '-n', '1', 'L', '%h', prog, '--cntldir', cntldir, '--id',
            dataset_id, '--prefix', prefixdir, '--buf', buf_size, crc_flag
        ]
        argv.append(downnodes_spaced)
        output = self.parallel_exec(argv=argv, runnodes=upnodes)[0]
        return output

    def jobstep_id(self, pid=-1):
        # allow launched job to show in apstat
        sleep(10)
        output = runproc(['apstat', '-avv'], getstdout=True)[0].split('\n')
        nid = None
        try:
            with open('/proc/cray_xt/nid', 'r') as NIDfile:
                nid = NIDfile.read()[:-1]
        except:
            return None
        pid = str(pid)
        currApid = None
        for line in output:
            line = line.strip()
            fields = re.split('\s+', line)
            if len(fields) < 8:
                continue
            if fields[0].startswith('Ap'):
                currApid = fields[2][:-1]
            elif fields[1].startswith('Originator:'):
                #did we find the apid that corresponds to the pid?
                # also check to see if it was launched from this MOM node in case two
                # happen to have the same pid
                thisnid = fields[5][:-1]
                if thisnid == nid and fields[7] == pid:
                    break
                currApid = None
        return currApid

    # Only use akill to kill the jobstep if desired and jobstep_id was successful
    def kill_jobstep(self, jobstep=None):
        # it looks like the Popen.terminate is working with srun
        if type(jobstep) is str:
            runproc(argv=['apkill', jobstep])
        else:
            super().kill_jobstep(jobstep)
