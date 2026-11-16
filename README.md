# entservices-mediaanddrm

Following plugins are moved to dedicated repositories as per below table:

| Plugin | Repository |
| --- | --- |
| cryptography | https://github.com/rdkcentral/entservices-cryptography |
| dtv | https://github.com/rdkcentral/entservices-dtv |
| linearplaybackcontrol | https://github.com/rdkcentral/entservices-linearplaybackcontrol |
| opencdmi | https://github.com/rdkcentral/entservices-opencdmi |
| playerinfo | https://github.com/rdkcentral/entservices-playerinfo |
| screencapture | https://github.com/rdkcentral/entservices-screencapture |
| unifiedcasmanagement | https://github.com/rdkcentral/entservices-unifiedcasmanagement |

Any further code changes need to come from the above repositories on the `develop` branch.
Ongoing release changes for `8.0`, `8.1`, `8.2`, `8.3`, and `8.4` branches should still use this repository.

## SystemAudioPlayer power transitions

SystemAudioPlayer registers as a PowerManager pre-change client and stops active
players before acknowledging standby, light sleep, deep sleep, or OFF transitions.
Player shutdown runs on the worker pool, not the PowerManager notification thread.
Other power states are acknowledged immediately.

For deep sleep and OFF, it reserves 5 seconds before queuing shutdown. A separate
monitor thread requests 2-second extensions one second before each reservation
expires, up to a total budget of 30 seconds (the final extension is capped at
1 second to stay within that budget). Fast
shutdown cancels the monitor and acknowledges completion immediately; a hung
shutdown cannot request delays indefinitely. PowerManager disconnection or plugin
deinitialization stops and joins the monitor, removes the pre-change client, and
unregisters notifications. Queued work from a previous PowerManager connection
cannot acknowledge transactions on a new connection.
Superseding power requests also cancel the previous monitor and discard its queued
shutdown work, so a wakeup does not execute a still-queued deep-sleep stop.
