import os

from scrjob.common import scr_prefix
from scrjob.resmgrs import AutoResourceManager
from scrjob.scr_param import SCR_Param
from scrjob.launchers import AutoJobLauncher
from scrjob.cli.scr_nodes_file import SCRNodesFile


class JobEnv:
    """The JobEnv class tracks information relating to the SCR job environment.

    This class retrieves information from the environment.
    This class contains pointers to the active Joblauncher, ResourceManager, and SCR_Param.

    References to these other classes should be assigned following instantiation of this class.

    Attributes
    ----------
    prefix     - string, SCR_PREFIX value, initialized upon init or through scr_prefix()
    param      - class, a reference to SCR_Param to read SCR param values
    resmgr     - class, a reference to ResourceManager to query resource manager
    launcher   - class, a reference to Joblauncher for MPI job launcher
    """

    def __init__(self, prefix=None, param=None, resmgr=None, launcher=None):
        # record SCR_PREFIX directory, default to scr_prefix if not provided
        self.prefix = prefix
        if prefix is None:
            self.prefix = scr_prefix()

        # used to read SCR parameter values,
        # which may be from environment or config files
        self.param = param
        if param is None:
            self.param = SCR_Param()

        # resource manager to query job id and node list
        self.resmgr = resmgr
        if resmgr is None:
            self.resmgr = AutoResourceManager()

        # job launcher for MPI jobs
        if launcher is None:
            self.launcher = AutoJobLauncher()
        else:
            self.launcher = AutoJobLauncher(launcher)

    def user(self):
        """Return the username from the environment."""
        return os.environ.get('USER')

    def node_list(self):
        """Return the SCR_NODELIST, if set, or None."""
        nodelist = os.environ.get('SCR_NODELIST')
        return self.resmgr.expand_hosts(nodelist)

    def dir_prefix(self):
        """Return the scr prefix."""
        return self.prefix

    def dir_scr(self):
        """Return the prefix/.scr directory."""
        return os.path.join(self.prefix, '.scr')

    def dir_dset(self, d):
        """Given a dataset id, return the dataset directory within the prefix
        prefix/.scr/scr.dataset.<id>"""
        return os.path.join(self.dir_scr(), 'scr.dataset.' + str(d))

    def runnode_count(self):
        """Return the number of nodes used in the last run, if known."""
        nodes_file = SCRNodesFile(prefix=self.prefix)
        num_nodes = nodes_file.last_num_nodes()
        return num_nodes if num_nodes is not None else 0
