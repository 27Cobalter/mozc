"""Prints Bazel workspace status values (see --workspace_status_command).

STABLE_BUILD_TIME: always printed. Shown in the About dialog.
STABLE_LOCAL_BUILD_NUMBER: printed only with --local. It is incremented on
every call and used as the 4th number of the version so that an installer built
locally overwrites the previously installed one (see --config local_build).
"""

import os
import sys
import time

print('STABLE_BUILD_TIME', time.strftime('%Y-%m-%d %H:%M:%S'))

if '--local' in sys.argv:
  counter = os.path.join(os.path.expanduser('~'), '.mozc_local_build_number')
  try:
    with open(counter, encoding='utf-8') as f:
      number = int(f.read().strip())
  except (OSError, ValueError):
    number = 100  # The REVISION of the dev channel.
  number = min(number + 1, 0xFFFF)  # Version fields are 16 bits.
  with open(counter, 'w', encoding='utf-8') as f:
    f.write(str(number))
  print('STABLE_LOCAL_BUILD_NUMBER', number)
