import os


def list_dir(user=None, jobid=None, base=False, runcmd=None, jobenv=None):
    """This method returns info on the SCR control/cache/prefix directories for
    the current user and jobid.

    Required Parameters
    ----------
    runcmd     string, 'control' or 'cache'
    jobenv     class, an instance of JobEnv with valid references to
               jobenv.resmgr and jobenv.param

    Returns
    -------
    string
      A space separated list of directories

    Error
    -----
      This method raises a RuntimeError
    """

    # check that user specified "control" or "cache"
    if runcmd != 'control' and runcmd != 'cache':
        raise RuntimeError(
            'list_dir: INVALID: \'control\' or \'cache\' must be specified.')

    # ensure jobenv is set
    if jobenv is None or jobenv.resmgr is None or jobenv.param is None:
        raise RuntimeError('list_dir: INVALID: Unknown environment.')

    # get the base directory
    bases = []
    if runcmd == 'cache':
        # lookup cache base
        cachedesc = jobenv.param.get_hash('CACHE')
        if type(cachedesc) is dict:
            bases = list(cachedesc.keys())
        elif cachedesc is not None:
            bases = [cachedesc]
        else:
            raise RuntimeError(
                'list_dir: INVALID: Unable to get parameter CACHE.')
    else:
        # lookup cntl base
        bases = jobenv.param.get('SCR_CNTL_BASE')
        if type(bases) is dict:
            bases = list(bases.keys())
        elif type(bases) is not None:
            bases = [bases]
        else:
            raise RuntimeError(
                'list_dir: INVALID: Unable to get parameter SCR_CNTL_BASE.')

    if len(bases) == 0:
        raise RuntimeError('list_dir: INVALID: Length of bases [] is zero.')

    # get the user/job directory
    suffix = ''
    if base == False:
        # if not specified, read username from environment
        if user is None:
            user = jobenv.user()

        # if not specified, read jobid from environment
        if jobid is None:
            jobid = jobenv.resmgr.job_id()

        # check that the required environment variables are set
        if user is None or jobid is None:
            # something is missing, print invalid dir and exit with error
            raise RuntimeError(
                'list_dir: INVALID: Unable to determine user or jobid.')

        suffix = os.path.join(user, 'scr.' + jobid)

    # ok, all values are here, print out the directory name and exit with success
    dirs = []
    for abase in bases:
        if suffix != '':
            dirs.append(os.path.join(abase, suffix))
        else:
            dirs.append(abase)
    return dirs
