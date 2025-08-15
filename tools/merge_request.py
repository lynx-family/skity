#!/usr/bin/env python3
# Copyright 2020 The Lynx Authors. All rights reserved.
import subprocess
import os

class MergeRequest:
  target_branch = 'main'
  def __init__(self, target_branch):
    self.target_branch = target_branch
    pass

  def RunCommand(self, command):
    p = subprocess.Popen(' '.join(command),
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         shell=True)
    result, error = p.communicate()
    # Compatibale with Python 3.
    # Since the return value of communicate is bytes instead of str in Python 3.
    return result.decode('utf-8'), error.decode('utf-8')

  # Get project/git root directory
  def GetRootDirectory(self):
    command = ['git', 'rev-parse', '--show-toplevel']
    result, error = self.RunCommand(command)
    if error:
      print(('Error, can not get top directory, make sure it is a git repo: %s'
              % (error)))
      return None
    return result.strip()

  # Get uncommitted changed files.
  def GetChangedFiles(self):
    file_list = []
    # Staged files
    command = ['git', 'diff', '{}..HEAD'.format(self.target_branch), '--name-only', '--diff-filter=d']
    result, error = self.RunCommand(command)
    if error:
      print(('Error, can not get staged files, make sure it is a git repo: %s'
              % (error)))
    for filename in result.split('\n'):
      filename = filename.strip()
      # convert to absolute path
      if not os.path.isabs(filename):
        filename = os.path.join(self.GetRootDirectory(), filename)
      if filename and filename != '':
        file_list.append(filename)
    # Unstaged files
    command = ['git', 'diff', '--name-only', '--diff-filter=ACMRT']
    result, error = self.RunCommand(command)
    if error:
      print(('Error, can not get staged files, make sure it is a git repo: %s'
              % (error)))
    for filename in result.split('\n'):
      filename = filename.strip()
      # convert to absolute path
      if not os.path.isabs(filename):
        filename = os.path.join(self.GetRootDirectory(), filename)
      if filename and filename != '':
        if filename not in file_list:
          file_list.append(filename)
    return file_list

  # Get changed files of last commit.
  def GetLastCommitFiles(self):
    command = ['git', 'diff', '--name-only', 'HEAD^', 'HEAD', '--diff-filter=ACMRT']
    result, error = self.RunCommand(command)
    if error:
      print(('Error: can not get change list of last commit: %s' % (error)))
      return []
    file_list = []
    for filename in result.split('\n'):
      filename = filename.strip()
      if filename and filename != '':
        file_list.append(filename)
    return file_list

  # Get commit log of last commit.
  def GetCommitLog(self):
    command = ['git', 'log', '--format=%B', '-n', '1', 'HEAD^..HEAD']
    result, error = self.RunCommand(command)
    if error:
      print('Error: can not get the commit log of last change.')
      return None
    return result

  # Get all file in the repo.
  def GetAllFiles(self):
    command = ['git', 'ls-tree', '--full-tree', '-r', '--name-only', 'HEAD']
    result, error = self.RunCommand(command)
    if error:
      print('Error: can not get all files, please check it is a git repo.')
      return None
    file_list = []
    for filename in result.split('\n'):
      filename = filename.strip()
      if filename and filename != '':
        file_list.append(filename)
    return file_list


if __name__ == '__main__':
  mr = MergeRequest()
  # print((mr.GetRootDirectory()))
  print((mr.GetChangedFiles()))
  # print((mr.GetCommitLog()))
  # print((mr.GetAllFiles()))
