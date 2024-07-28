## @file
#  GitHub API helper functions.
#
#  Copyright (c) Microsoft Corporation.
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

import logging
import re
import requests

from collections import OrderedDict
from edk2toollib.utility_functions import RunCmd, RunPythonScript
from github import Auth, Github, GithubException
from io import StringIO
from typing import List


"""GitHub API helper functions."""


def _authenticate(token: str):
    """Authenticate to GitHub using a token.

    Args:
        token (str): The GitHub token to use for authentication.

    Returns:
        Github: A GitHub instance.
    """
    auth = Auth.Token(token)
    return Github(auth=auth)


def _get_pr(token: str, owner: str, repo: str, pr_number: str):
    """Get the PR object from GitHub.

    Args:
        token (str): The GitHub token to use for authentication.
        owner (str): The GitHub owner (organization) name.
        repo (str): The GitHub repository name (e.g. 'edk2').
        pr_number (str): The pull request number.

    Returns:
        PullRequest: The PullRequest object.
    """
    try:
        g = _authenticate(token)
        return g.get_repo(f"{owner}/{repo}").get_pull(pr_number)
    except GithubException as ge:
        print(f"::error title=Error Getting PR {pr_number} Info!::"
              f"{ge.data['message']}")
        return None


def leave_pr_comment(
    token: str, owner: str, repo: str, pr_number: str, comment_body: str
):
    """Leaves a comment on a PR.

    Args:
        token (str): The GitHub token to use for authentication.
        owner (str): The GitHub owner (organization) name.
        repo (str): The GitHub repository name (e.g. 'edk2').
        pr_number (str): The pull request number.
        comment_body (str): The comment text. Markdown is supported.
    """
    if pr := _get_pr(token, owner, repo, pr_number):
        try:
            pr.create_issue_comment(comment_body)
        except GithubException as ge:
            print(f"::error title=Error Commenting on PR {pr_number}!::"
                  f"{ge.data['message']}")


def get_reviewers_for_range(
    workspace_path: str,
    maintainer_file_path: str,
    range_start="master",
    range_end: str = "HEAD",
) -> List[str]:
    """Get the reviewers for the current branch.

       !!! note
           This function accepts a range of commits and returns the reviewers
           for that set of commits as a single list of GitHub usernames. To get
           the reviewers for a single commit, set `range_start` and `range_end`
           to the commit SHA.

    Args:
        workspace_path (str): The workspace path.
        maintainer_file_path (str): The maintainer file path.
        range_start (str, optional): The range start ref. Defaults to "master".
        range_end (str, optional): The range end ref. Defaults to "HEAD".

    Returns:
        List[str]: A list of GitHub usernames.
    """
    if range_start == range_end:
        commits = [range_start]
    else:
        commit_stream_buffer = StringIO()
        cmd_ret = RunCmd(
            "git",
            f"log --format=format:%H {range_start}..{range_end}",
            workingdir=workspace_path,
            outstream=commit_stream_buffer,
            logging_level=logging.INFO,)
        if cmd_ret != 0:
            print(
                f"::error title=Commit Lookup Error!::Error getting branch "
                f"commits: [{cmd_ret}]: {commit_stream_buffer.getvalue()}")
            return []
        commits = commit_stream_buffer.getvalue().splitlines()

    raw_reviewers = []
    for commit_sha in commits:
        reviewer_stream_buffer = StringIO()
        cmd_ret = RunPythonScript(
            maintainer_file_path,
            f"-g {commit_sha}",
            workingdir=workspace_path,
            outstream=reviewer_stream_buffer,
            logging_level=logging.INFO)
        if cmd_ret != 0:
            print(
                f"::error title=Reviewer Lookup Error!::Error calling "
                f"GetMaintainer.py: [{cmd_ret}]: "
                f"{reviewer_stream_buffer.getvalue()}")
            return []

        commit_reviewers = reviewer_stream_buffer.getvalue()

        pattern = r"\[(.*?)\]"
        matches = re.findall(pattern, commit_reviewers)
        if not matches:
            return []

        print(
            f"::debug title=Commit {commit_sha[:7]} "
            f"Reviewer(s)::{', '.join(matches)}")

        raw_reviewers.extend(matches)

    reviewers = list(OrderedDict.fromkeys([r.strip() for r in raw_reviewers]))

    print(f"::debug title=Total Reviewer Set::{', '.join(reviewers)}")

    return reviewers


def get_pr_sha(token: str, owner: str, repo: str, pr_number: str) -> str:
    """Returns the commit SHA of given PR branch.

       This returns the SHA of the merge commit that GitHub creates from a
       PR branch. This commit contains all of the files in the PR branch in
       a single commit.

    Args:
        token (str): The GitHub token to use for authentication.
        owner (str): The GitHub owner (organization) name.
        repo (str): The GitHub repository name (e.g. 'edk2').
        pr_number (str): The pull request number.

    Returns:
        str: The commit SHA of the PR branch. An empty string is returned
             if the request fails.
    """
    url = f"https://api.github.com/repos/{owner}/{repo}/pulls/{pr_number}"
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github.v3+json",
    }
    response = requests.get(url, headers=headers)
    try:
        response.raise_for_status()
    except requests.exceptions.HTTPError:
        print(
            f"::error title=HTTP Error!::Error getting PR Commit Info: "
            f"{response.reason}")
        return ""

    commit_sha = response.json()["merge_commit_sha"]

    print(f"::notice title=PR {pr_number} Old Merge Commit SHA::{commit_sha}")

    pr = _get_pr(token, owner, repo, pr_number)
    if pr:
        merge_commit_sha = pr.merge_commit_sha
        print(f"::notice title=PR {pr_number} New Merge Commit SHA::{merge_commit_sha}")

    return commit_sha


def add_reviewers_to_pr(
    token: str, owner: str, repo: str, pr_number: str, user_names: List[str]
) -> List[str]:
    """Adds the set of GitHub usernames as reviewers to the PR.

    Args:
        token (str): The GitHub token to use for authentication.
        owner (str): The GitHub owner (organization) name.
        repo (str): The GitHub repository name (e.g. 'edk2').
        pr_number (str): The pull request number.
        user_names (List[str]): List of GitHub usernames to add as reviewers.

    Returns:
        List[str]: A list of GitHub usernames that were successfully added as
                   reviewers to the PR. This list will exclude any reviewers
                   from the list provided if they are not relevant to the PR.
    """
    try:
        g = _authenticate(token)
        pr = g.get_repo(f"{owner}/{repo}").get_pull(pr_number)
    except GithubException as ge:
        print(f"::error title=Error Getting PR {pr_number} Info!::"
              f"{ge.data['message']}")
        return

    # The pull request author cannot be a reviewer.
    pr_author = pr.user.login.strip()

    # The current reviewers of the PR do not need to be requested again.
    current_pr_reviewers = pr.get_review_requests()[0]

    # A user can only be added if they are a collaborator of the repository.
    repo_collaborators = [c.login.strip() for c in repo.get_collaborators()]
    non_collaborators = [u for u in user_names if u not in repo_collaborators]

    excluded_pr_reviewers = [pr_author] + current_pr_reviewers + non_collaborators
    new_pr_reviewers = [u for u in user_names if u not in excluded_pr_reviewers]

    # Notify the admins of the repository if non-collaborators are requested.
    if non_collaborators:
        repo_admins = repo.get_collaborators(permission='admin')

        print(
            f"::warning title=Non-Collaborator Reviewers Found!::"
            f"{', '.join(non_collaborators)}")

        leave_pr_comment(
            token,
            owner,
            repo,
            pr_number,
            f"&#9888; **WARNING: Cannot add some reviewers**: A user  "
            f"specified as a reviewer for this PR is not a collaborator "
            f"of the edk2 repository. Please add them as a collaborator to "
            f"the repository so they can be requested in the future.\n\n"
            f"Non-collaborators requested:\n{', '.join(non_collaborators)}"
            f"\n\nAttn Admins:\n\n"
            f"{'\n'.join([f"- @{a.login}" for a in repo_admins])}")

    # Add any new reviewers to the PR if needed.
    if new_pr_reviewers:
        print(f"::debug title=Adding New PR Reviewers::"
              f"{', '.join(new_pr_reviewers)}")

        pr.create_review_request(reviewers=new_pr_reviewers)

    return new_pr_reviewers
