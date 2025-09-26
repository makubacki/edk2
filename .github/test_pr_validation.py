#!/usr/bin/env python3
"""
Local Test Script for PR Validation

This script allows local testing of the PR validation logic.
Use this to test different scenarios before deploying to GitHub Actions.
Also supports historical PR validation testing against a configurable
number of PRs.

Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent
"""

import argparse
import getpass
import os
import sys
import tempfile
from datetime import datetime
from typing import Dict, List
from unittest.mock import Mock, patch

# Add the scripts directory to the path so we can import the modules
script_dir = os.path.join(os.path.dirname(__file__), 'scripts')
sys.path.insert(0, script_dir)

# Import after path setup
import GitHub
import validate_pr_formatting


def create_mock_pr(title: str, body: str):
    """Create a mock PR object for testing."""
    mock_pr = Mock()
    mock_pr.title = title
    mock_pr.body = body
    return mock_pr


def create_test_template():
    """Create a sample PR template for testing."""
    return """## Description of changes

<_Add a description of your changes here_>

## Testing

<_How has this been tested?_>

## Checklist

- [ ] My code follows the coding style of this project
- [ ] I have performed a self-review of my code
"""


def test_validation_scenarios():
    """Test various validation scenarios."""
    test_cases = [{'name': 'Valid PR',
                   'title': 'Fix buffer overflow in memory allocator',
                   'body': 'This PR fixes a critical buffer overflow vulnerability in the DXE memory allocator. The issue was caused by insufficient bounds checking when allocating memory for variable-length structures. This change adds proper size validation and bounds checking to prevent heap corruption.',
                   'should_pass': True},
                  {'name': 'Empty title',
                   'title': '',
                   'body': 'This is a good description that is long enough to pass validation and provides meaningful context about the changes.',
                   'should_pass': False},
                  {'name': 'Empty body',
                   'title': 'Valid title',
                   'body': '',
                   'should_pass': False},
                  {'name': 'Body too short',
                   'title': 'Valid title',
                   'body': 'Short description.',
                   'should_pass': False},
                  {'name': 'Template lines left in body',
                   'title': 'Valid title',
                   'body': 'This PR fixes something important.\n\n<_Add a description of your changes here_>\n\nIt has been tested thoroughly.',
                   'should_pass': False}]

    print("🧪 Testing PR Validation Logic\n")

    for test_case in test_cases:
        print(f"Testing: {test_case['name']}")

        # Create validator with mock data
        validator = validate_pr_formatting.PRValidator(
            "fake_token", "owner", "repo", 123)

        # Mock the PR data fetch to return our test data
        with patch.object(validator, 'fetch_pr_data', return_value=(test_case['title'], test_case['body'])):
            with patch.object(validator, 'fetch_template_content', return_value=create_test_template()):
                is_valid = validator.validate_pr()

                if is_valid == test_case['should_pass']:
                    print(
                        f"  ✅ PASS - Expected: {test_case['should_pass']}, Got: {is_valid}")
                else:
                    print(
                        f"  ❌ FAIL - Expected: {test_case['should_pass']}, Got: {is_valid}")

                if validator.validation_messages:
                    print(f"  📝 Messages:")
                    for msg in validator.validation_messages:
                        print(f"    - {msg}")

                print()


def test_github_actions_output():
    """Test GitHub Actions output functionality."""
    print("🎬 Testing GitHub Actions Output\n")

    validator = validate_pr_formatting.PRValidator(
        "fake_token", "owner", "repo", 123)
    validator.validation_messages = ["Test error message"]

    # Create temporary files to simulate GitHub Actions environment
    with tempfile.NamedTemporaryFile(mode='w+', delete=False) as github_output:
        with tempfile.NamedTemporaryFile(mode='w+', delete=False) as github_env:
            # Set environment variables
            os.environ['GITHUB_OUTPUT'] = github_output.name
            os.environ['GITHUB_ENV'] = github_env.name

            # Test output functions
            validator.set_github_output('test_key', 'test_value')
            validator.set_github_env('TEST_VAR', 'test content\nwith newlines')

            # Read back the results
            github_output.seek(0)
            output_content = github_output.read()

            github_env.seek(0)
            env_content = github_env.read()

            print(f"GitHub Output file content:\n{output_content}")
            print(f"GitHub Environment file content:\n{env_content}")

            # Clean up
            os.unlink(github_output.name)
            os.unlink(github_env.name)

            if 'GITHUB_OUTPUT' in os.environ:
                del os.environ['GITHUB_OUTPUT']
            if 'GITHUB_ENV' in os.environ:
                del os.environ['GITHUB_ENV']


def get_github_auth() -> str:
    """Get GitHub authentication token from user input."""
    print("GitHub Authentication Required")
    print("To test against historical PRs, we need a GitHub token.")
    print("You can create a personal access token at:")
    print("https://github.com/settings/tokens")
    print("The token needs 'public_repo' permissions.")
    print()

    token = getpass.getpass("Enter your GitHub token: ").strip()
    if not token:
        print("❌ No token provided. Exiting.")
        sys.exit(1)

    return token


def fetch_recent_prs(token: str, owner: str, repo: str,
                     count: int = 50) -> List[Dict]:
    """Fetch recent PRs from the repository."""
    print(f"📡 Fetching {count} recent PRs from {owner}/{repo}...")

    try:
        # Use GitHub API to get recent merged PRs
        github = GitHub._authenticate(token)

        # Get the repo object
        repo_obj = github.get_repo(f"{owner}/{repo}")

        # Get recent closed PRs (merged ones)
        pulls = repo_obj.get_pulls(state='closed', sort='updated',
                                   direction='desc')

        pr_data = []
        fetched = 0

        for pr in pulls:
            if fetched >= count:
                break

            # Only include merged PRs
            if pr.merged:
                pr_data.append({
                    'number': pr.number,
                    'title': pr.title or '',
                    'body': pr.body or '',
                    'url': pr.html_url,
                    'merged_at': pr.merged_at,
                    'user': pr.user.login if pr.user else 'unknown'
                })
                fetched += 1

        print(f"✅ Successfully fetched {len(pr_data)} merged PRs")
        return pr_data

    except Exception as e:
        print(f"❌ Error fetching PRs: {e}")
        return []


def test_historical_prs(token: str, owner: str = "tianocore",
                        repo: str = "edk2", count: int = 50) -> Dict:
    """Test PR validation against historical PRs."""
    print(f"\n🕒 Testing Historical PR Validation")
    print(f"Repository: {owner}/{repo}")
    print(f"Testing against {count} recent merged PRs\n")

    # Fetch recent PRs
    prs = fetch_recent_prs(token, owner, repo, count)

    if not prs:
        print("❌ No PRs fetched. Cannot run historical tests.")
        return {'total': 0, 'passed': 0, 'failed': 0, 'results': []}

    results = []
    passed = 0
    failed = 0

    # Create template for testing
    template_content = create_test_template()

    print("Running validation tests...\n")

    for i, pr_data in enumerate(prs, 1):
        print(f"Testing PR #{pr_data['number']} ({i}/{len(prs)})", end=" ")

        try:
            # Create validator
            validator = validate_pr_formatting.PRValidator(
                token, owner, repo, pr_data['number'])

            # Mock the fetch methods to use our cached data
            with patch.object(validator, 'fetch_pr_data',
                              return_value=(pr_data['title'], pr_data['body'])):
                with patch.object(validator, 'fetch_template_content',
                                  return_value=template_content):
                    is_valid = validator.validate_pr()

            result = {
                'pr_number': pr_data['number'],
                'title': pr_data['title'],
                'user': pr_data['user'],
                'url': pr_data['url'],
                'merged_at': pr_data['merged_at'],
                'valid': is_valid,
                'messages': list(validator.validation_messages)
            }

            results.append(result)

            if is_valid:
                passed += 1
                print("✅")
            else:
                failed += 1
                print("❌")

        except Exception as e:
            print(f"💥 Error: {e}")
            failed += 1
            results.append({
                'pr_number': pr_data['number'],
                'title': pr_data['title'],
                'user': pr_data['user'],
                'url': pr_data['url'],
                'merged_at': pr_data['merged_at'],
                'valid': False,
                'messages': [f'Test error: {str(e)}']
            })

    return {
        'total': len(prs),
        'passed': passed,
        'failed': failed,
        'results': results
    }


def generate_markdown_report(test_results: Dict,
                             output_file: str = None) -> str:
    """Generate a markdown report of test results."""
    if output_file is None:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_file = f"pr_validation_report_{timestamp}.md"

    total = test_results['total']
    passed = test_results['passed']
    failed = test_results['failed']
    pass_rate = (passed / total * 100) if total > 0 else 0

    markdown = f"""# PR Validation Historical Test Report

**Generated:** {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}

## Summary

| Metric | Value |
|--------|-------|
| Total PRs Tested | {total} |
| Passed | {passed} |
| Failed | {failed} |
| Pass Rate | {pass_rate:.1f}% |

## Results

| PR # | Title | User | Status | Issues |
|------|-------|------|--------|--------|"""

    for result in test_results['results']:
        status = "✅ Pass" if result['valid'] else "❌ Fail"
        title_truncated = (
            result['title'][:50] + "...") if len(result['title']) > 50 else result['title']
        title_clean = title_truncated.replace('|', '\\|').replace('\n', ' ')

        issues = ""
        if not result['valid'] and result['messages']:
            issues = "; ".join(result['messages'])[:100]
            if len(issues) == 100:
                issues += "..."
            issues = issues.replace('|', '\\|').replace('\n', ' ')

        markdown += f"\n| [#{
            result['pr_number']}]({
            result['url']}) | {title_clean} | {
            result['user']} | {status} | {issues} |"

    if failed > 0:
        markdown += f"\n\n## Failed PR Details\n"
        for result in test_results['results']:
            if not result['valid']:
                markdown += f"\n### PR #{
                    result['pr_number']}: {
                    result['title']}\n"
                markdown += f"**User:** {result['user']}  \n"
                markdown += f"**URL:** {result['url']}  \n"
                markdown += f"**Issues:**\n"
                for msg in result['messages']:
                    markdown += f"- {msg}\n"
                markdown += "\n"

    # Write to file
    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(markdown)
        print(f"\n📄 Report saved to: {output_file}")
    except Exception as e:
        print(f"❌ Error saving report: {e}")

    return markdown


def test_comment_posting():
    """Test the integrated comment posting functionality."""
    print("💬 Testing Integrated Comment Posting\n")

    validator = validate_pr_formatting.PRValidator("fake_token", "owner",
                                                   "repo", 123)
    validator.validation_messages = [
        "⚠️ Pull request title cannot be empty.",
        "⚠️ Body is too short."
    ]

    # Mock the GitHub PR object
    mock_pr = Mock()
    mock_pr.get_issue_comments.return_value = []  # No existing comments

    with patch.object(validate_pr_formatting.GitHub, '_get_pr',
                      return_value=mock_pr):
        with patch.object(validate_pr_formatting.GitHub,
                          'leave_pr_comment') as mock_leave_comment:
            result = validator.post_validation_comment_with_dedup()

            print(f"Comment posting result: {result}")
            print(f"leave_pr_comment called: {mock_leave_comment.called}")
            if mock_leave_comment.called:
                call_args = mock_leave_comment.call_args[0]
                print(f"Comment body preview: {call_args[4][:100]}...")


def test_main_with_comment_flag():
    """Test the main function with --post-comment flag."""
    print("🚀 Testing Main Function with --post-comment Flag\n")

    # Mock sys.argv
    test_argv = ['validate_pr_formatting.py', '--post-comment']
    with patch.object(sys, 'argv', test_argv):
        mock_func = 'post_validation_comment_standalone'
        with patch.object(validate_pr_formatting, mock_func,
                          return_value=True) as mock_standalone:
            result = validate_pr_formatting.main()

            print(f"Main function result: {result}")
            print(f"Standalone function called: {mock_standalone.called}")


def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Test PR validation logic locally"
    )

    parser.add_argument(
        '--historical',
        action='store_true',
        help='Run historical PR validation tests'
    )

    parser.add_argument(
        '--count',
        type=int,
        default=50,
        help='Number of PRs to test (default: 50)'
    )

    parser.add_argument(
        '--owner',
        default='tianocore',
        help='Repository owner (default: tianocore)'
    )

    parser.add_argument(
        '--repo',
        default='edk2',
        help='Repository name (default: edk2)'
    )

    parser.add_argument(
        '--output',
        help='Output file for markdown report'
    )

    return parser.parse_args()


def main():
    """Main function with enhanced argument parsing."""
    args = parse_arguments()

    if args.historical:
        # Historical PR testing mode
        print("🐍 PR Validation Historical Test Suite\n")
        print("This mode tests PR validation against historical PRs.")
        print("GitHub authentication is required.\n")

        # Get GitHub token
        token = get_github_auth()

        try:
            # Run historical tests
            results = test_historical_prs(token, args.owner, args.repo,
                                          args.count)

            if results['total'] > 0:
                # Generate and display report
                markdown_report = generate_markdown_report(results,
                                                           args.output)

                # Display summary
                print(f"\n📊 Test Summary:")
                print(f"   Total PRs: {results['total']}")
                print(f"   Passed: {results['passed']}")
                print(f"   Failed: {results['failed']}")

                pass_rate = (results['passed'] / results['total'] * 100
                             if results['total'] > 0 else 0)
                print(f"   Pass Rate: {pass_rate:.1f}%")

                if results['failed'] > 0:
                    print(f"\n❌ {results['failed']} PRs failed validation.")
                    print("See the detailed report for more information.")
                else:
                    print("\n✅ All PRs passed validation!")
            else:
                print("❌ No PRs were tested.")
                return 1

        except Exception as e:
            print(f"❌ Historical test failed: {e}")
            import traceback
            traceback.print_exc()
            return 1

    else:
        # Standard local testing mode
        print("🐍 PR Validation Local Test Suite\n")
        print("This script tests the PR validation logic locally "
              "without GitHub API calls.\n")

        try:
            test_validation_scenarios()
            test_github_actions_output()
            test_comment_posting()
            test_main_with_comment_flag()
            print("✅ All tests completed!")

        except Exception as e:
            print(f"❌ Test failed with error: {e}")
            import traceback
            traceback.print_exc()
            return 1

    return 0


if __name__ == '__main__':
    exit_code = main()
    sys.exit(exit_code)
