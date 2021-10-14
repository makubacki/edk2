## @file
# Compare builds from different git branches/refs.
#
# Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#

'''
CompareBuild
'''
from __future__ import print_function
import os
import sys
import argparse
from git import Repo
import filecmp
from shutil import copytree
from shutil import rmtree
from shutil import move
import subprocess

#
# Globals for help information
#
__prog__        = 'CompareBuild'
__copyright__   = 'Copyright (c) 2021, Intel Corporation. All rights reserved.'
__description__ = 'Compare two builds from different git branches/refs.\n'

class dircmp(filecmp.dircmp):
    """
    Compare the content of dir1 and dir2. In contrast with filecmp.dircmp, this
    subclass compares the content of files with the same path.
    """
    def phase3(self):
        """
        Find out differences between common files.
        Ensure we are using content comparison with shallow=False.
        """
        fcomp = filecmp.cmpfiles(self.left, self.right, self.common_files,
                                 shallow=False)
        self.same_files, self.diff_files, self.funny_files = fcomp

def is_same(dir1, dir2, verbose = False):
    NumSame = 0
    NumIgnored = 0;
    NumDifferent = 0
    compared = dircmp(dir1, dir2)
    NumDifferent += (len(compared.left_only) + len(compared.right_only) + len(compared.funny_files))
    if verbose:
        for file in (compared.left_only + compared.right_only + compared.funny_files):
            print ('Difference detected in file:', os.path.join (dir1, file))
    for file in compared.diff_files:
        if file == 'Build.log':
            if verbose:
                print ('Ignore differences in file:', os.path.join (dir1, file))
            NumIgnored += 1
            continue
        if file.startswith('GlobalVar_') and file.endswith('.bin'):
            if verbose:
                print ('Ignore differences in file:', os.path.join (dir1, file))
            NumIgnored += 1
            continue
        Extension = os.path.splitext(file)[1]
        if Extension in ['.map', '.pdb', '.obj', '.lib', '.i', '.ii', '.iii']:
            if verbose:
                print ('Ignore differences in file:', os.path.join (dir1, file))
            NumIgnored += 1
            continue
        if Extension in ['.asm']:
            File = open(os.path.join(dir1, file))
            Lines1 = File.readlines()
            File.close()
            File = open(os.path.join(dir2, file))
            Lines2 = File.readlines()
            File.close()
            Lines1 = [x for x in Lines1 if x.lstrip() and x.lstrip()[0] != ';']
            Lines2 = [x for x in Lines2 if x.lstrip() and x.lstrip()[0] != ';']
            if Lines1 != Lines2:
                if verbose:
                    print ('Difference detected in file:', os.path.join (dir1, file))
                NumDifferent += 1
                continue
            if verbose:
                print ('Identical file:', os.path.join (dir1, file))
            NumSame += 1
            continue
        if verbose:
            print ('Difference detected in file:', os.path.join (dir1, file))
        NumDifferent += 1
    for subdir in compared.common_dirs:
        SubNumSame, SunNumIgnored, SubNumDifferent = is_same(os.path.join(dir1, subdir), os.path.join(dir2, subdir), verbose=verbose)
        NumSame      += SubNumSame
        NumIgnored   += SunNumIgnored
        NumDifferent += SubNumDifferent
    return NumSame, NumIgnored, NumDifferent

def Cleanup (args, BuildDir):
    if BuildDir:
        if args.Cleanup and os.path.exists (os.path.join (BuildDir, 'Build.Ref1')):
            print ('rmtree:', os.path.join (BuildDir, 'Build.Ref1'))
            rmtree (os.path.join (BuildDir, 'Build.Ref1'))
        if args.Cleanup and os.path.exists (os.path.join (BuildDir, 'Build.Ref2')):
            print ('rmtree:', os.path.join (BuildDir, 'Build.Ref2'))
            rmtree (os.path.join (BuildDir, 'Build.Ref2'))
        if os.path.exists (BuildDir + '.temp'):
            if os.path.exists (BuildDir):
                rmtree (BuildDir)
            os.rename(BuildDir + '.temp', BuildDir)

if __name__ == '__main__':
    Workspace = ''
    NewConfDir = ''
    BuildDir = ''
    ActiveCommit = ''

    #
    # Create command line argument parser object
    #
    parser = argparse.ArgumentParser (prog = __prog__,
                                      description = __description__ + __copyright__,
                                      conflict_handler = 'resolve')
    parser.add_argument ("--ref1", dest = 'Ref1', required = True,
                         help = "First GIT branch/reference.")
    parser.add_argument ("--ref2", dest = 'Ref2', required = True,
                         help = "Second GIT branch/reference.")
    parser.add_argument ("--skip-build", dest = 'SkipBuild', action = "store_true",
                         help = "Skip build and only redo compare from most recent build.")
    parser.add_argument ("--cleanup", dest = 'Cleanup', action = "store_true",
                         help = "Cleanup temporary Build and Conf folders.")
    parser.add_argument ("--verbose", dest = 'Verbose', action = "store_true",
                         help = "Increase output messages")
    parser.add_argument('rest', nargs=argparse.REMAINDER)
    #
    # Parse command line arguments
    #
    args = parser.parse_args ()
    if not args.Ref1 or not args.Ref2:
        sys.exit('ERROR: Invalid ref')
    if args.Ref1 == args.Ref2:
        print('Builds identical.  Same refs.')
        sys.exit(0)
    BuildCommand = ' '.join(args.rest)
    if BuildCommand == '':
        sys.exit('ERROR: No build command provided')

    try:
        repo = Repo('.', search_parent_directories=True)
    except:
        sys.exit('ERROR:Not in a git repo')
    if repo.is_dirty():
        sys.exit('ERROR:Repo has uncommitted changes')
    if repo.head.is_detached:
        ActiveCommit = repo.head.commit
    else:
        ActiveCommit = repo.active_branch.name
    
    Workspace = os.environ['WORKSPACE']
    if not os.path.exists (Workspace):
        sys.exit('ERROR: WORKSPACE %s is invalid' % Workspace)
    print ('WORKSPACE:', Workspace)
    BuildDir = os.path.join (Workspace, 'Build')
    print ('Build Directory:', BuildDir)
    ConfDir = os.path.join (Workspace, 'Conf')
    if not os.path.exists (ConfDir):
        Cleanup (args, BuildDir)
        sys.exit('ERROR: No Conf directory')
        
    # cleanup from previous run
    if os.path.exists (BuildDir + '.temp'):
        if os.path.exists (BuildDir):
            rmtree (BuildDir)
        os.rename(BuildDir + '.temp', BuildDir)
    if os.path.exists (BuildDir + '.Ref1'):
        print ('rmtree:', BuildDir + '.Ref1')
        rmtree (BuildDir + '.Ref1')
    if os.path.exists (BuildDir + '.Ref2'):
        print ('rmtree:', BuildDir + '.Ref2')
        rmtree (BuildDir + '.Ref2')

    if not args.SkipBuild:
        for (Ref,Ext) in [(args.Ref1,'.Ref1'), (args.Ref2,'.Ref2')]:
            # Checkout Ref and run build
            try:
                print ('Checkout:', Ref)
                repo.git.checkout(Ref)
                repo.git.submodule('update', '--init')
            except:
                Cleanup (args, BuildDir)
                if ActiveCommit:
                    print ('Checkout:', ActiveCommit)
                    repo.git.checkout(ActiveCommit)
                sys.exit('Checkout ref %s failed' % (Ref))

            # Preserve exisiting Build dir
            if os.path.exists (BuildDir):
                os.rename(BuildDir, BuildDir + '.temp')
            # Create Build dir
            os.mkdir(BuildDir)
            
            # Create modified Conf directory
            NewConfDir = os.path.join (Workspace, 'Build', 'Conf.temp')
            copytree(ConfDir, NewConfDir)
            BuildRuleTxt = os.path.join (NewConfDir, 'build_rule.txt')
            if not os.path.exists (BuildRuleTxt):
                Cleanup (args, BuildDir)
                sys.exit('No build_rules.txt file')
            ToolsDefTxt = os.path.join (NewConfDir, 'tools_def.txt')
            if not os.path.exists (ToolsDefTxt):
                Cleanup (args, BuildDir)
                sys.exit('No tools_def.txt file')

            # Create modified build_rule.txt file
            File = open(BuildRuleTxt, 'r')
            BuildRuleLines = File.readlines()
            File.close()
            NewBuildRuleLines = []
            for Line in BuildRuleLines:
                if Line.strip().startswith ('#'):
                    NewBuildRuleLines.append(Line + '\n')
                    continue
                Line.replace('"$(SLINK)" cr ', '"$(SLINK)" crD ', 1)
                NewBuildRuleLines.append(Line)
            File = open(BuildRuleTxt, 'w')
            File.writelines(NewBuildRuleLines)
            File.close()

            # Create modified tools_def.txt file
            File = open(ToolsDefTxt, 'r')
            ToolsDefLines = File.readlines()
            File.close()
            NewToolsDefLines = []
            for Line in ToolsDefLines:
                if Line.strip().startswith ('#'):
                    NewToolsDefLines.append(Line + '\n')
                    continue
                Line = Line.rstrip()
                if '_VS20' in Line and '_CC_FLAGS' in Line:
                    Line += ' -D__TIME__="00:00:00" -D__DATE__="Jan 1 2021" /wd4117 -DDEBUG_LINE_NUMBER=1 -DDEBUG_EXPRESSION_STRING_VALUE=\\"Hello\\" /Fa'
                    Line += ' /Brepro'
                if '_VS20' in Line and ('_SLINK_FLAGS' in Line or '_DLINK_FLAGS' in Line or '_ASM_FLAGS' in Line):
                    Line += ' /Brepro'
                if '_VS20' in Line and '_NASM_FLAGS' in Line:
                    Line += ' --reproducible'
                if '_CLANGPDB_' in Line and ('_CC_FLAGS' in Line or '_SLINK_FLAGS' in Line or '_DLINK_FLAGS' in Line or '_ASM_FLAGS' in Line):
                    Line += ' -Brepro'
                if '_CLANGPDB_' in Line and '_NASM_FLAGS' in Line:
                    Line += ' --reproducible'
                if '_GCC5_' in Line and  '_CC_FLAGS' in Line:
                    Line += ' -D__TIME__=\'"00:00:00"\' -D__DATE__=\'"Jan 1 2021"\' -Wno-builtin-macro-redefined -frandom-seed=CONSTANTSEEDSTRING'
                if '_GCC5_' in Line and '_NASM_FLAGS' in Line:
                    Line += ' --reproducible'
                if '_VFR_FLAGS' in Line:
                    Line = Line.replace ('-l', '')
                NewToolsDefLines.append(Line + '\n')
            File = open(ToolsDefTxt, 'w')
            File.writelines(NewToolsDefLines)
            File.close()
            
            # Run build command using customized Conf directory
            print ('Run:', BuildCommand + ' --conf=' + NewConfDir)
            subprocess.run(BuildCommand + ' --conf=' + NewConfDir, shell=True)
            if os.path.exists (BuildDir):
                os.rename(BuildDir, BuildDir + Ext)
            if os.path.exists (BuildDir + '.temp'):
                os.rename(BuildDir + '.temp', BuildDir)
            if os.path.exists (os.path.join (BuildDir, 'Build' + Ext)):
                rmtree (os.path.join (BuildDir, 'Build' + Ext))
            if os.path.exists (BuildDir + Ext):
                move (BuildDir + Ext, os.path.join (BuildDir, 'Build' + Ext))

        # Restore active git branch
        if ActiveCommit:
            print ('Checkout:', ActiveCommit)
            repo.git.checkout(ActiveCommit)

    # Determine if build output dirs are the same
    if not os.path.exists (os.path.join (BuildDir, 'Build.Ref1')) or not os.path.exists (os.path.join (BuildDir, 'Build.Ref2')):
        Cleanup (args, BuildDir)
        print('Build did not generate expected build output directories')
        sys.exit('Builds not identical')
    NumSame, NumIgnored, NumDifferent = is_same(os.path.join (BuildDir, 'Build.Ref1'), os.path.join (BuildDir, 'Build.Ref2'), args.Verbose)
    if NumSame > 0 and NumDifferent == 0:
        Cleanup (args, BuildDir)
        print('Builds identical')
        print('  Number same      =', NumSame)
        print('  Number ignored   =', NumIgnored)
        print('  Number different =', NumDifferent)
        sys.exit(0)
    else:
        Cleanup (args, BuildDir)
        print('Builds not identical')
        print('  Number same      =', NumSame)
        print('  Number ignored   =', NumIgnored)
        print('  Number different =', NumDifferent)
        sys.exit('Builds not identical')
