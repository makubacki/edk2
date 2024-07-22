import logging
import re
import requests
from edk2toollib.utility_functions import RunCmd
from collections import OrderedDict
from io import StringIO


def leave_pr_comment(token, owner, repo, pr_number, comment_body):
    url = f"https://api.github.com/repos/{owner}/{repo}/issues/{pr_number}/comments"
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github.v3+json"
    }
    data = {
        "body": comment_body
    }
    response = requests.post(url, json=data, headers=headers)
    response.raise_for_status()
    return response.json()

def get_reviewers_for_current_branch(workspace_path, maintainer_file_path):
    out_stream_buffer = StringIO()
    cmd_ret = RunCmd("python", f"{maintainer_file_path} -g",
                     workingdir=workspace_path,
                     outstream=out_stream_buffer,
                     logging_level=logging.INFO
                     )
    if cmd_ret != 0:
        print(f"::error title=Reviewer Lookup Error!::Error calling GetMaintainer.py: [{cmd_ret}]: {out_stream_buffer.getvalue()}")
        return

    raw_reviewers = out_stream_buffer.getvalue().strip()

    pattern = r'\[(.*?)\]'
    matches = re.findall(pattern, raw_reviewers)
    if not matches:
        return []
    matches = list(OrderedDict.fromkeys([match.strip() for match in matches]))
    return matches

# token only needed if the file is private
def download_gh_file(github_url, local_path, token=None):
    headers = {}
    if token:
        headers["Authorization"] = f"Bearer {token}"

    try:
        response = requests.get(github_url, headers=headers)
        response.raise_for_status()
    except requests.exceptions.HTTPError:
        print(f"::error title=HTTP Error!::Error downloading {github_url}: {response.reason}")
        return

    with open(local_path, 'w', encoding='utf-8') as file:
        file.write(response.text)

def get_user_ids(token, usernames):
    user_ids = []
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github.v3+json"
    }
    for username in usernames:
        response = requests.get(f'https://api.github.com/users/{username}', headers=headers)
        response.raise_for_status()
        user_ids.append(response.json()['id'])
    return user_ids

def add_reviewers_to_pr(token, owner, repo, pr_number, user_names):
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github.v3+json"
    }
    url = f"https://api.github.com/repos/{owner}/{repo}/pulls/{pr_number}/requested_reviewers"
    data = {
        "reviewers": user_names
    }
    response = requests.post(url, json=data, headers=headers)
    try:
        response.raise_for_status()
    except requests.exceptions.HTTPError:
        if response.status_code == 422 and "Reviews may only be requested from collaborators" in response.json().get("message"):
            print(f"::error title=User is not a Collaborator!::{response.json().get('message')}")
            leave_pr_comment(token, owner, repo, pr_number,
                             f"&#9888; **WARNING: Cannot add reivewers**: A user specified as a "
                             f"reviewer for this PR is not a collaborator "
                             f"of the edk2 repository. Please add them as a collaborator to the "
                             f"repository and re-request the review.\n\n"
                             f"Users requested:\n{', '.join(user_names)}")
        elif response.status_code == 422:
            print("::error title=Invalid Request!::The request is invalid. "
                  "Verify the API request string.")
    return response.json()
