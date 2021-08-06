#! /usr/bin/env python3
"""
resmgr/auto.py
AutoResourceManager is called to return the appropriate ResourceManager class

This is the class called to obtain an instance of a resource manager.

To add a new resource manager:
* Insert the new class name at the end of pyfe.resmgr import statement
* Insert the condition and return statement for your new resource manager within __new__

The resmgr string normally comes from the value of scr_const.SCR_RESOURCE_MANAGER
"""

from pyfe import scr_const

from pyfe.resmgr import (
    ResourceManager,
    LSF,
    PBSALPS,
    #PMIX,
    SLURM,
    FLUX,
)


class AutoResourceManager:
  def __new__(cls, resmgr=None):
    if resmgr is None:
      resmgr = scr_const.SCR_RESOURCE_MANAGER
    try:
      resmgr = FLUX()
      return resmgr
    except:
      pass
    if resmgr == 'SLURM':
      return SLURM()
    if resmgr == 'LSF':
      return LSF()
    if resmgr == 'APRUN':
      return APRUN()
    #if resmgr=='PMIX':
    #  return PMIX()
    return ResourceManager()


if __name__ == '__main__':
  resourcemgr = AutoResourceManager()
  print(type(resourcemgr))
  resourcemgr = AutoResourceManager(resmgr='SLURM')
  print(type(resourcemgr))
  resourcemgr = AutoResourceManager(resmgr='LSF')
  print(type(resourcemgr))
  resourcemgr = AutoResourceManager(resmgr='APRUN')
  print(type(resourcemgr))
  #resourcemgr = AutoResourceManager(resmgr='PMIX')
  #print(type(resourcemgr))
