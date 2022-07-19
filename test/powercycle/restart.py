import json
import os
import time
import urllib.request

from ansible import context
from ansible.cli import CLI
from ansible.executor.playbook_executor import PlaybookExecutor
from ansible.inventory.manager import InventoryManager
from ansible.module_utils.common.collections import ImmutableDict
from ansible.parsing.dataloader import DataLoader
from ansible.vars.manager import VariableManager


def runPlaybook(yml, evars):
    loader = DataLoader()
    context.CLIARGS = ImmutableDict(tags={}, timeout=300, listtags=False, listtasks=False, listhosts=False, syntax=False, connection='ssh',
                    module_path=None, forks=100, private_key_file=None, ssh_common_args=None, ssh_extra_args=None,
                    sftp_extra_args=None, scp_extra_args=None, become=False, become_method='sudo', become_user='root', 
                    verbosity=True, check=False, start_at_task=None, extra_vars=[{**{"ansible_python_interpreter": "/usr/bin/python3"}, **evars}])
    inventory = InventoryManager(loader=loader, sources=('hosts',))
    variable_manager = VariableManager(loader=loader, inventory=inventory, version_info=CLI.version_info(gitinfo=False))
    pbex = PlaybookExecutor(playbooks=[yml], inventory=inventory, variable_manager=variable_manager, loader=loader, passwords={})
    results = pbex.run()
    if results != 0:
        raise RuntimeError("Playbook failed to run correctly")

def switch(plug, on):
    if on:
        state = "On"
    else:
        state = "Off"
    api_url = f"http://{plug}/cm?cmnd=Power%20{state}"
    request = urllib.request.Request(api_url)
    with urllib.request.urlopen(request) as response:
        data = json.loads(response.read().decode("utf-8"))
        if data['POWER'] != state.upper():
            raise Exception("Failed to set power", data['POWER'])

if __name__ == "__main__":

    plug = os.getenv('TASMOTA')
    if plug is None:
        raise Exception("Need to set TASMOTA env variable")

    COUNT = 3
    for i in range(COUNT):
        print(f"At {i}")
        if i < COUNT - 1:
            runPlaybook("control.yml", {"shutdown" : True}) 
            time.sleep(20)
            switch(plug, False)
            time.sleep(5)
            switch(plug, True)
        else:
            runPlaybook("control.yml", {}) 
