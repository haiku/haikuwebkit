# Copyright (C) 2018-2023 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import argparse
import configparser
import hashlib
import json
import logging
import os
import subprocess
import sys
import tempfile
import urllib
from datetime import datetime
from importlib.util import spec_from_file_location, module_from_spec

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner
from webkitpy.benchmark_runner.browser_driver.browser_driver_factory import BrowserDriverFactory
from webkitpy.benchmark_runner.run_benchmark import default_browser, default_platform, benchmark_runner_subclasses
from webkitpy.benchmark_runner.utils import get_path_from_project_root
from webkitpy.benchmark_runner.webserver_benchmark_runner import WebServerBenchmarkRunner

_log = logging.getLogger(__name__)
BROWSERPERFDASH_DIR = os.path.dirname(os.path.abspath(__file__))


def parse_args():
    parser = argparse.ArgumentParser(description='Automate the browser based performance benchmarks')
    # browserperfdash specific arguments.
    parser.add_argument('--config-file', dest='config_file', default=None, required=True, help='Configuration file for sending the results to the performance dashboard server(s).')
    parser_group_browser_version = parser.add_mutually_exclusive_group(required=True)
    parser_group_browser_version.add_argument('--browser-version', dest='browser_version', default=None, help='A string that identifies the browser version.')
    parser_group_browser_version.add_argument('--query-browser-version', dest='query_browser_version', action='store_true', help='Try to automatically query the browser version.')
    # arguments shared with run-benchmark.
    parser.add_argument('--build-directory', dest='buildDir', help='Path to the browser executable (e.g. WebKitBuild/Release/).')
    parser.add_argument('--platform', dest='platform', default=None, choices=BrowserDriverFactory.available_platforms())
    parser.add_argument('--browser', dest='browser', default=None, choices=BrowserDriverFactory.available_browsers())
    parser.add_argument('--driver', default=None, choices=benchmark_runner_subclasses.keys(), help='Use the specified benchmark driver. Defaults to %s.' % WebServerBenchmarkRunner.name)
    parser.add_argument('--local-copy', dest='localCopy', help='Path to a local copy of the benchmark (e.g. PerformanceTests/SunSpider/).')
    parser.add_argument('--count', dest='countOverride', type=int, help='Number of times to run the benchmark (e.g. 5).')
    parser.add_argument('--timeout', dest='timeoutOverride', type=int, help='Number of seconds to wait for the benchmark to finish (e.g. 600).')
    parser.add_argument('--save-results-directory', help='Optional: a directory where to keep a copy of the json results that are sent to the remote server.')
    parser_group_timestamp = parser.add_mutually_exclusive_group(required=False)
    parser_group_timestamp.add_argument('--timestamp', dest='timestamp', type=int, help='Date of when the benchmark was run that will be sent to the performance dashboard server. Format is Unix timestamp (second since epoch). Optional. The server will use as date "now" if not specified.')
    parser_group_timestamp.add_argument('--timestamp-from-repo', dest='local_path_to_repo', default=None, help='Use the commit date of the checked-out git commit (HEAD) on the specified local path as the date to send to the server of when the benchmark was run. Useful for running bots in parallel (or even re-testing old checkouts) and ensuring that the timestamps of the benchmark data on the server gets the same value than the commit date of the checked-out commit on the given repository.')
    parser_group_plan_selection = parser.add_mutually_exclusive_group(required=True)
    parser_group_plan_selection.add_argument('--plan', dest='plan', help='Benchmark plan to run. e.g. speedometer, jetstream')
    parser_group_plan_selection.add_argument('--list-plans', action='store_true', help='List the available plans')
    parser_group_plan_selection.add_argument('--read-results-json', help='Instead of running a benchmark, format the output saved in JSON_FILE.')
    parser_group_plan_selection.add_argument('--plans-from-config', action='store_true', help='Run the list of plans specified in the config file.')
    parser_group_plan_selection.add_argument('--allplans', action='store_true', help='Run all available benchmark plans sequentially')
    parser.add_argument('browser_args', nargs='*', help='Additional arguments to pass to the browser process. These are positional arguments and must follow all other arguments. If the pass through arguments begin with a dash, use `--` before the argument list begins.')
    args = parser.parse_args()
    if args.save_results_directory and not os.path.isdir(args.save_results_directory):
        parser.error(f'The results directory "{args.save_results_directory}" does not exist')
    return args


class BrowserPerfDashRunner(object):
    name = 'browserperfdash_runner'

    def __init__(self, args):
        self._args = args
        self._benchmark_runner_plan_directory = os.path.abspath(BenchmarkRunner.plan_directory())
        self._browserperfdash_runner_plan_directory = os.path.abspath(os.path.join(BROWSERPERFDASH_DIR, 'plans'))
        for plan_dir in [self._benchmark_runner_plan_directory, self._browserperfdash_runner_plan_directory]:
            if not os.path.isdir(plan_dir):
                raise Exception(f"Can't find plan directory: {plan_dir}")
        self._benchmark_runner_available_plans = BenchmarkRunner.available_plans()
        self._benchmark_runner_available_plans_split_subtests = self._get_benchmark_runner_split_subtests_plans(self._benchmark_runner_available_plans)
        self._browserperfdash_runner_available_plans = self._load_and_list_plan_plugins()
        self._available_plans = sorted(self._benchmark_runner_available_plans + self._benchmark_runner_available_plans_split_subtests + list(self._browserperfdash_runner_available_plans.keys()))
        self._parse_config_file(self._args.config_file)
        self._set_args_default_values_from_config_file()
        self._browser_driver = BrowserDriverFactory.create(self._args.platform, self._args.browser, self._args.browser_args)
        # This is the dictionary that will be sent as the HTTP POST request that browserperfdash expects
        # (as defined in https://github.com/Igalia/browserperfdash/blob/master/docs/design-document.md)
        # - The bot_* data its obtained from the config file
        # - the browser_* data is given at startup time via command-line arguments
        # - The test_* data is generated after each test run.
        self._result_data = {'bot_id': None,
                             'bot_password': None,
                             'browser_id': self._args.browser,
                             'browser_version': self._args.browser_version,
                             'local_hostname': os.uname().nodename,
                             'test_id': None,
                             'test_version': None,
                             'test_data': None}
        if args.timestamp:
            self._result_data['timestamp'] = args.timestamp
            date_str = datetime.fromtimestamp(self._result_data['timestamp']).isoformat()
            _log.info('Will send the benchmark data as if it was generated on date: {date}'.format(date=date_str))
        elif args.local_path_to_repo is not None:
            if not os.path.isdir(args.local_path_to_repo):
                raise ValueError('The value "{path}" to the local repository is not a valid directory'.format(path=args.local_path_to_repo))
            _log.info('Checking the timestamp of the git commit checked-out (HEAD) on the repository at path "{repo_path}"'.format(repo_path=args.local_path_to_repo))
            timestamp = subprocess.check_output(['git', 'log', '-1', '--pretty=%ct', 'HEAD'], cwd=args.local_path_to_repo)
            timestamp = timestamp.decode('utf-8').strip()
            if not timestamp.isnumeric():
                raise ValueError('The git command to find the timestamp on the HEAD git commit returned an unexpected string: {timestamp}'.format(timestamp=timestamp))
            timestamp = int(timestamp)
            if timestamp < 0:
                raise ValueError('The git command to find the timestamp on the HEAD git commit returned a negative timestamp: {timestamp}'.format(timestamp=timestamp))
            date_str = datetime.fromtimestamp(timestamp).isoformat()
            _log.info('Will send the benchmark data as if it was generated on date: {date}'.format(date=date_str))
            self._result_data['timestamp'] = timestamp

        # Get the browser version if needed
        if self._args.query_browser_version:
            self._result_data['browser_version'] = self._browser_driver.browser_version()
            if not self._result_data['browser_version']:
                raise NotImplementedError('The driver for browser {browser} does not implement a way to automatically obtain the version. Please specify the version manually.'.format(browser=self._args.browser))
        _log.info('Browser version is: {browser_version}'.format(browser_version=self._result_data['browser_version']))

    # The precedence order for setting the value for the arguments is:
    # 1 - What the user defines via command line switches (current_value is not None).
    # 2 - What is defined on the config file inside the "[settings]" section.
    # 3 - The default_value passed here or None.
    def _get_value_from_current_or_settings_or_default(self, current_value, variable_name, default_value=None, allowed_value_list=None, check_if_is_int=False):
        if current_value is not None:
            return current_value
        if self._config_parser.has_option('settings', variable_name):
            settings_value = self._config_parser.get('settings', variable_name)
            if allowed_value_list is not None:
                if settings_value not in allowed_value_list:
                    raise ValueError('The "{variable_name}" value "{settings_value}" defined in the config file "{config_file}" is not valid. Allowed values are: "{allowed_values}"'.format(
                                     variable_name=variable_name, settings_value=settings_value, config_file=self._args.config_file, allowed_values=' '.join(allowed_value_list)))
            if check_if_is_int:
                if not settings_value.isdigit():
                    raise ValueError('The "{variable_name}" value "{settings_value}" defined in the config file "{config_file}" is not valid. Only integer values are allowed'.format(
                                     variable_name=variable_name, settings_value=settings_value, config_file=self._args.config_file))
                settings_value = int(settings_value)
            return settings_value
        return default_value

    def _set_args_default_values_from_config_file(self):
        self._args.timeoutOverride = self._get_value_from_current_or_settings_or_default(self._args.timeoutOverride, 'timeout', check_if_is_int=True)
        self._args.countOverride = self._get_value_from_current_or_settings_or_default(self._args.countOverride, 'count', check_if_is_int=True)
        self._args.platform = self._get_value_from_current_or_settings_or_default(self._args.platform, 'platform', default_value=default_platform(), allowed_value_list=BrowserDriverFactory.available_platforms())
        self._args.browser = self._get_value_from_current_or_settings_or_default(self._args.browser, 'browser', default_value=default_browser(), allowed_value_list=BrowserDriverFactory.available_browsers())
        self._args.driver = self._get_value_from_current_or_settings_or_default(self._args.driver, 'driver', default_value=WebServerBenchmarkRunner.name, allowed_value_list=benchmark_runner_subclasses.keys())

    def _parse_config_file(self, config_file):
        if not os.path.isfile(config_file):
            raise Exception('Can not open config file for uploading results at: {config_file}'.format(config_file=config_file))
        self._config_parser = configparser.RawConfigParser()
        self._config_parser.read(config_file)
        # Validate the data
        found_one_valid_server = False
        for section in self._config_parser.sections():
            if section == 'settings':
                continue
            found_one_valid_server = True
            for required_key in ['bot_id', 'bot_password', 'post_url']:
                if not self._config_parser.has_option(section, required_key):
                    raise ValueError('Missed key "{required_key}" for server "{server_name}" in config file "{config_file}"'.format(
                                     required_key=required_key, server_name=section, config_file=config_file))
        if not found_one_valid_server:
            raise ValueError('At least one server should be defined on the config file "{config_file}"'.format(config_file=config_file))

    # As version of the test we calculate a hash of the contents
    def _get_plan_version_hash(self, plan_name):
        version_hash = hashlib.md5()
        plan_name = plan_name.removesuffix('-split-subtests')
        if self._is_benchmark_runner_plan(plan_name):
            plan_file_path = os.path.join(self._benchmark_runner_plan_directory, f'{plan_name}.plan')
        else:
            plan_file_path = self._browserperfdash_runner_available_plans[plan_name]['path']
        with open(plan_file_path, 'r') as plan_fd:
            plan_content = plan_fd.read().encode('utf-8', errors='ignore')
            version_hash.update(plan_content)
            # For benchmark runner plans we load the json and we add the hashes of patches and other imported plans
            if self._is_benchmark_runner_plan(plan_name):
                plan_dict = json.loads(plan_content)
                if 'import_plan_file' in plan_dict:
                    imported_plan_file = os.path.join(self._benchmark_runner_plan_directory, plan_dict.pop('import_plan_file'))
                    with open(imported_plan_file, 'r') as imported_plan_fd:
                        imported_plan_content = imported_plan_fd.read().encode('utf-8', errors='ignore')
                        version_hash.update(imported_plan_content)
                        imported_plan_dict = json.loads(imported_plan_content)
                    imported_plan_dict.update(plan_dict)
                    plan_dict = imported_plan_dict
                plan_patches = []
                for plan_key in plan_dict:
                    if plan_key.endswith('_patch'):
                        plan_patches.append(get_path_from_project_root(plan_dict[plan_key]))
                    if plan_key.endswith('_patches'):
                        for plan_patch in plan_dict[plan_key]:
                            plan_patches.append(get_path_from_project_root(plan_patch))
                for patch_file_path in plan_patches:
                    if not os.path.isfile(patch_file_path):
                        raise Exception('Can not find patch file "{patch_file_path}" referenced in plan "{plan_file_path}"'.format(patch_file_path=patch_file_path, plan_file_path=plan_file_path))
                    with open(patch_file_path, 'r') as patch_fd:
                        version_hash.update(patch_fd.read().encode('utf-8', errors='ignore'))
        return version_hash.hexdigest()

    def _get_test_data_json_string(self, temp_result_file):
        temp_result_file.flush()
        temp_result_file.seek(0)
        temp_result_json = json.load(temp_result_file)
        if 'debugOutput' in temp_result_json:
            del temp_result_json['debugOutput']
        return json.dumps(temp_result_json)

    def _get_benchmark_runner_split_subtests_plans(self, benchmark_runner_plans):
        split_subtests_plans = []
        for plan in benchmark_runner_plans:
            assert (not plan.endswith('-split-subtests')), 'Name collision, a plan should not end with the string "-split-subtests"'
            if BenchmarkRunner.available_subtests(plan):
                split_subtests_plans.append(f'{plan}-split-subtests')
        return split_subtests_plans

    def _load_and_list_plan_plugins(self):
        plugins = {}
        for filename in os.listdir(self._browserperfdash_runner_plan_directory):
            if filename.endswith('.py') and not filename.startswith('_'):
                plugin_path = os.path.join(self._browserperfdash_runner_plan_directory, filename)
                module_name = os.path.splitext(filename)[0]
                spec = spec_from_file_location(module_name, plugin_path)
                module = module_from_spec(spec)
                sys.modules[module_name] = module
                spec.loader.exec_module(module)
                if hasattr(module, "register"):
                    plugin = module.register()
                    for required_key in ['name', 'run']:
                        if required_key not in plugin:
                            raise ValueError(f'Required key "{required_key}" not found in plugin defined at "{plugin_path}"')
                    plugin_name = plugin['name']
                    if plugin_name in plugins:
                        raise ValueError(f'Plugin "{plugin_name}" defined in "{plugin_path}" was already defined by other plugin.')
                    plugins[plugin_name] = {'run': plugin['run'], 'path': plugin_path}
                else:
                    _log.warning(f'Plugin {filename} has no register() function. Ignoring plugin')
        return plugins

    # urllib.request.urlopen always raises an exception when the http return code is not 200
    # so this wraps the call to return the HTTPError object instead of raising the exception.
    # The HTTPError object can be treated later as a http.client.HTTPResponse object.
    def _send_post_request_data(self, post_url, post_data):
        try:
            return urllib.request.urlopen(post_url, post_data)
        except urllib.error.HTTPError as e:
            return e

    # browserperfdash uses code 202 for handling long StreamingHttpResponses that ouput text meanwhile
    # processing to avoid timeouts and then put the real reply at the end.
    def _read_stream_response(self, post_request, server_name):
        try:
            reply_code = post_request.getcode()
            if reply_code != 202:
                return reply_code, post_request.read().decode('utf-8', errors='ignore')
            post_reply = b''
            last_status_update = datetime.now()
            while True:
                chunk = post_request.read(1)
                if not chunk:
                    break
                post_reply += chunk
                seconds_elapased_since_status_update = (datetime.now() - last_status_update).total_seconds()
                # Each minute log an status update to avoid timeouts on the worker side due to not output
                if seconds_elapased_since_status_update > 60:
                    _log.info('Waiting for server {server_name} to process the results from {test_name} and browser {browser_name} version {browser_version} ...'.format(
                              server_name=server_name, test_name=self._result_data['test_id'], browser_name=self._result_data['browser_id'], browser_version=self._result_data['browser_version']))
                    last_status_update = datetime.now()
            post_reply = post_reply.decode('utf-8', errors='ignore')
            recording_reply_msg = False
            reply_msg = ''
            for line in post_reply.splitlines():
                if line.startswith('HTTP_202_FINAL_STATUS_CODE') and '=' in line:
                    reply_code = int(line.split("=")[1].strip())
                if recording_reply_msg:
                    reply_msg += line + '\n'
                if line.startswith('HTTP_202_FINAL_MSG_NEXT_LINES'):
                    recording_reply_msg = True
            reply_msg = reply_msg.strip()
            return reply_code, reply_msg
        except Exception as e:
            reply_code = 400
            reply_msg = ('Exception when trying to read the response from server {server_name}: {e}'.format(server_name=server_name, e=str(e)))
            return reply_code, reply_msg

    def _upload_result(self):
        upload_failed = False
        for server in self._config_parser.sections():
            if server == 'settings':
                continue
            self._result_data['bot_id'] = self._config_parser.get(server, 'bot_id')
            self._result_data['bot_password'] = self._config_parser.get(server, 'bot_password')
            post_data = urllib.parse.urlencode(self._result_data).encode('utf-8')
            post_url = self._config_parser.get(server, 'post_url')
            try:
                post_request = self._send_post_request_data(post_url, post_data)
                reply_code, reply_msg = self._read_stream_response(post_request, server)
                if reply_code == 200:
                    _log.info('Sucesfully uploaded results to server {server_name} for test {test_name} and browser {browser_name} version {browser_version}'.format(
                               server_name=server, test_name=self._result_data['test_id'], browser_name=self._result_data['browser_id'], browser_version=self._result_data['browser_version']))
                else:
                    upload_failed = True
                    _log.error('The server {server_name} returned an error code: {http_error}'.format(server_name=server, http_error=reply_code))
                    _log.error('The error text from the server {server_name} was: "{error_text}"'.format(server_name=server, error_text=reply_msg))
            except Exception as e:
                upload_failed = True
                _log.error('Exception while trying to upload results to server {server_name}'.format(server_name=server))
                _log.error(e)
        return not upload_failed

    def _is_benchmark_runner_plan(self, plan):
        return plan in self._benchmark_runner_available_plans

    def is_browserperfdash_runner_plan(self, plan):
        return plan in self._browserperfdash_runner_available_plans

    def available_plans(self):
        return self._available_plans

    def _run_benchmark_runner_plan(self, plan, result_file_path):
        assert(self._is_benchmark_runner_plan(plan)), f'plan {plan} is not a valid benchmark-runner plan'
        benchmark_runner_class = benchmark_runner_subclasses[self._args.driver]
        runner = benchmark_runner_class(plan,
                                        self._args.localCopy,
                                        self._args.countOverride,
                                        self._args.timeoutOverride,
                                        self._args.buildDir,
                                        result_file_path,
                                        self._args.platform,
                                        self._args.browser,
                                        None,
                                        browser_args=self._args.browser_args)
        runner.execute()

    def _run_benchmark_runner_plan_split_subtests(self, plan, result_file_path):
        plan = plan.removesuffix('-split-subtests')
        assert(self._is_benchmark_runner_plan(plan)), f'plan {plan} is not a valid benchmark-runner plan'
        subtests_to_run = BenchmarkRunner.format_subtests(BenchmarkRunner.available_subtests(plan))
        total_subtests = len(subtests_to_run)
        _log.info(f'Running the {total_subtests} subtests of benchmark plan {plan} one-by-one')
        benchmark_name = None
        split_subtest_data = {}
        subtests_passed = []
        subtests_failed = []
        for current_subtest_index, subtest in enumerate(subtests_to_run):
            _log.info(f'Running subtest {subtest} of benchmark plan {plan} [{current_subtest_index} of {total_subtests}]')
            benchmark_runner_class = benchmark_runner_subclasses[self._args.driver]
            with tempfile.NamedTemporaryFile() as subtest_temp_result_file:
                try:
                    runner = benchmark_runner_class(plan,
                                                    self._args.localCopy,
                                                    self._args.countOverride,
                                                    self._args.timeoutOverride,
                                                    self._args.buildDir,
                                                    subtest_temp_result_file.name,
                                                    self._args.platform,
                                                    self._args.browser,
                                                    None,
                                                    subtests=[subtest],
                                                    browser_args=self._args.browser_args)
                    runner.execute()
                except Exception as e:
                    _log.error(f'subtest {subtest} of benchmark plan {plan} failed with exception: {e}')
                    subtests_failed.append(subtest)
                    continue
                subtest_temp_result_file.flush()
                subtest_temp_result_file.seek(0)
                temp_result_json = json.load(subtest_temp_result_file)
                if 'debugOutput' in temp_result_json:
                    del temp_result_json['debugOutput']

                benchmark_name_list = list(temp_result_json.keys())
                assert(len(benchmark_name_list) == 1), "There is more than one main benchmark name in the result data, this is unexpected"
                if not split_subtest_data:
                    # This runs only on the first iteration: build the split_subtest_data initial dict and set the value of benchmark_name
                    benchmark_name = benchmark_name_list[0]
                    split_subtest_data[benchmark_name] = {'metrics': temp_result_json[benchmark_name]['metrics'], 'tests': {}}
                else:
                    assert(benchmark_name == benchmark_name_list[0]), f'Benchmark name should not change between subtests, expected "{benchmark_name}" but got "{benchmark_name_list[0]}"'
                    assert(split_subtest_data[benchmark_name]['metrics'] == temp_result_json[benchmark_name]['metrics']), 'Metrics aggregation entry should not change between subtests'

                benchmark_subtests = list(temp_result_json[benchmark_name]['tests'].keys())
                assert(len(benchmark_subtests) == 1), "There is more than one subtest in the data, this is unexpected"
                subtest_name = benchmark_subtests[0]
                assert(subtest_name not in split_subtest_data[benchmark_name]['tests']), 'Data for subtest {subtest_name} is repeated'
                split_subtest_data[benchmark_name]['tests'][subtest_name] = temp_result_json[benchmark_name]['tests'][subtest_name]
                subtests_passed.append(subtest)

        if not subtests_passed:
            raise RuntimeError(f'All subtests of benchmark plan {plan} failed: {subtests_failed}')
        # Append 'split-subtests' to the benchmark name and save the joint data.
        split_subtest_data[f'{benchmark_name}-split-subtests'] = split_subtest_data.pop(benchmark_name)
        print(split_subtest_data)
        with open(result_file_path, 'w') as f:
            json.dump(split_subtest_data, f, indent=2)
            f.write("\n")

        _log.info(f'The following subtests of benchmark plan {plan} passed: {subtests_passed}')
        if len(subtests_failed) > 0:
            _log.error(f'The following subtests of benchmark plan {plan} failed: {subtests_failed}')
        return len(subtests_failed)

    def _run_plan(self, plan, result_file_path):
        if self._is_benchmark_runner_plan(plan):
            return self._run_benchmark_runner_plan(plan, result_file_path)
        if self.is_browserperfdash_runner_plan(plan):
            return self._browserperfdash_runner_available_plans[plan]['run'](self._browser_driver, result_file_path)
        if plan.endswith('-split-subtests'):
            return self._run_benchmark_runner_plan_split_subtests(plan, result_file_path)
        raise NotImplementedError(f'Implementation missing to run plan {plan}')

    def _save_to_results_directory(self, plan, upload_worked):
        stamp = datetime.now().strftime("%Y%m%dT%H%M%S")
        worked_str = 'worked' if upload_worked else 'failed'
        copy_file_name = f'{self._args.browser}_{self._result_data["browser_version"]}_{plan}_{self._result_data["test_version"]}_{stamp}_upload-{worked_str}.json'
        copy_file_path = os.path.join(self._args.save_results_directory, copy_file_name)
        _log.info(f'Saving results to {copy_file_path}')
        with open(copy_file_path, "w") as f:
            json.dump(self._result_data, f, indent=2)
            f.write("\n")

    def run(self):
        failed = []
        worked = []
        warned = []
        skipped = []
        plan_list = []
        if self._args.plan:
            if self._args.plan not in self.available_plans():
                raise Exception(f"Can't find a a plan named '{self._args.plan}'")
            plan_list = [self._args.plan]
        elif self._args.allplans:
            plan_list = self.available_plans()
            skippedfile = os.path.join(self._benchmark_runner_plan_directory, 'Skipped')
            if not plan_list:
                raise Exception('Can\'t find any plan in the directory {plan_directory}'.format(plan_directory=self._benchmark_runner_plan_directory))
            if os.path.isfile(skippedfile):
                skipped = [line.strip() for line in open(skippedfile) if not line.startswith('#') and len(line) > 1]
        elif self._args.plans_from_config:
            if not self._config_parser.has_option('settings', 'plan_list'):
                raise Exception('Can\'t find a "plan_list" key inside a "settings" section on the config file.')
            plan_list_from_config_str = self._config_parser.get('settings', 'plan_list')
            plan_list_from_config = plan_list_from_config_str.split()
            _log.info('Read a list of {number_plans} plans from the config file: "{plan_list}"'.format(number_plans=len(plan_list_from_config), plan_list=' '.join(plan_list_from_config)))
            available_plans = self.available_plans()
            for plan in plan_list_from_config:
                if plan in available_plans:
                    if plan not in plan_list:
                        plan_list.append(plan)
                else:
                    _log.error('Plan "{plan}" is not in the list of known plans: "{known_plan_list}"'.format(plan=plan, known_plan_list=' '.join(available_plans)))
                    failed.append(plan)
            _log.info('Running {number_plans} plans: "{plan_list}"'.format(number_plans=len(plan_list), plan_list=' '.join(plan_list)))

        if len(plan_list) < 1:
            _log.error('No benchmarks plans available to run in directory {plan_directory}'.format(plan_directory=self._benchmark_runner_plan_directory))
            return max(1, len(failed))

        _log.info('Starting benchmark for browser {browser}'.format(browser=self._args.browser))

        iteration_count = 0
        for plan in plan_list:
            iteration_count += 1
            if plan in skipped:
                _log.info('Skipping benchmark plan: {plan_name} because is listed on the Skipped file [benchmark {iteration} of {total}]'.format(plan_name=plan, iteration=iteration_count, total=len(plan_list)))
                continue
            _log.info('Starting benchmark plan: {plan_name} [benchmark {iteration} of {total}]'.format(plan_name=plan, iteration=iteration_count, total=len(plan_list)))
            try:
                with tempfile.NamedTemporaryFile() as temp_result_file:
                    self._result_data['local_timestamp_teststart'] = datetime.now().strftime('%s')
                    number_failed_subtests = self._run_plan(plan, temp_result_file.name)
                    subtests_failed = type(number_failed_subtests) is int and number_failed_subtests > 0
                    self._result_data['local_timestamp_testend'] = datetime.now().strftime('%s')
                    _log.info('Finished benchmark plan: {plan_name}'.format(plan_name=plan))
                    # Fill test info for upload
                    self._result_data['test_id'] = plan
                    self._result_data['test_version'] = self._get_plan_version_hash(plan)
                    # Fill obtained test results for upload
                    self._result_data['test_data'] = self._get_test_data_json_string(temp_result_file)
                    _log.info(f'Uploading results for plan: {plan} and browser {self._args.browser} version {self._result_data["browser_version"]}')
                    upload_worked = self._upload_result()
                    if upload_worked:
                        (warned if subtests_failed else worked).append(plan)
                    else:
                        failed.append(plan)
                    if self._args.save_results_directory:
                        self._save_to_results_directory(plan, upload_worked)
            except KeyboardInterrupt:
                raise
            except Exception as e:
                failed.append(plan)
                _log.exception(f'Error running benchmark plan: {plan}')
                _log.error(e)

        retcode = 0
        if len(worked) > 0:
            _log.info(f'The following benchmark plans have been upload succesfully: {worked}')

        if len(warned) > 0:
            _log.warning(f'The following benchmark plans have been upload succesfully but had subtests failing: {warned}')
            retcode += len(warned)

        if len(failed) > 0:
            _log.error(f'The following benchmark plans have failed to run or to upload: {failed}')
            retcode += len(failed)

        return retcode


def format_logger(logger):
    logger.setLevel(logging.INFO)
    ch = logging.StreamHandler()
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    ch.setFormatter(formatter)
    logger.addHandler(ch)


def read_results_json(file_path):
    if not os.path.isfile(file_path):
        raise ValueError(f"Can't find file {file_path}")
    with open(file_path, 'r') as f:
        json_data = json.load(f)
    is_browserperfdash_runner_json_result = "test_data" in json_data
    if is_browserperfdash_runner_json_result:
        print('---- TEST RUN INFO ----')
        for key in json_data.keys():
            if key != "test_data":
                print(f"{key} = {json_data[key]}")
    print('---- TEST RESULTS -----')
    results_json = json.loads(json_data["test_data"]) if is_browserperfdash_runner_json_result else json_data
    if 'debugOutput' in results_json:
        del results_json['debugOutput']
    BenchmarkRunner.show_results(results_json, True, False)
    return 0


def main():
    args = parse_args()
    if args.read_results_json:
        return read_results_json(args.read_results_json)
    perfdashrunner = BrowserPerfDashRunner(args)
    if args.list_plans:
        print("Available plans: ")
        for plan in perfdashrunner.available_plans():
            print("\t%s" % plan)
        return 0
    return perfdashrunner.run()
