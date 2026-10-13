We need a new plugin, that adds a new tab called "Servers"

Developers should be able to define servers that their code depends on, or is interested in.
And in the GUI see which significant servers are running, and answering pings.

## Config

The functionality of this tab should be based on `.ini` files like `geanyagent` module.

As with filetypetools & agenttools, `servertools.conf`, this should be override-able in the project's own `./config/` directory.
Project's `./config/` should also support `servertools_extra.conf`, that adds additional project specific configurations but does not override the defauts.
The idea is thatproject's `./config/` can start indexing at `_0` and not conflict with core|default servers.


```
[servers]
server_0=🕋 nginx
name_0=nginx
type_0=systemd
changes_0=reload src/www;src/cgi

server_1=🧱 firewall
name_1=ufw
type_1=systemd
```


Attributes supported per server type..,.

- **server** - name of the server (for systemd server this is sufficient information)  if it is emty thismeans dont show ths server (server `server_0=` to remove a sever froma  specific project)
- **type** - system | systemd | local  - (TODO Windows & Mac support)
- **start** - script to start the server
- **status** - script to check the status of the server, i.e. up or down
- **start** - script to start the server
- **reload** - script to reload the server, may not be stop start, e.g. nginx supports hot reloads
- **changes** - first arg never (default) | reboot | reload  how to restart the server, 2nd arg is  `;` separated list of folders to watch for changes, if there are changes reload the server using the configured method.
- **icon** - .png for the UI
- **ping** - command to test the servers is up
- **log** -  file name of the main log file.
- **sudo** - sudo command to run the server as root, if this is missing or blank current user starts and stops servers (n.b. system handles this with popups anyway)
- **on_open** - when opening the project in geany, start|stop|reload the server
- **on_close** - when opening the project in geany, start|stop|reload the server


## UI

UI for servers should be a table of data, one line per configured server, use utf-8 as migh as possible.

### Columns 

- status - show a green, or red dot for the server status, for systemd this is abase on `systemctl status` ourput
- active - show green or yellow if the last ping was succcesful
- actions - buttons to start,stop,reload,ping
- last boot - timestam of last time we restarted the server (if we did not boot it this vale should be blank)
- logs - if we have a configured log file, there should be a button to `tail -f` the logs in a VTE in the geanycli module.

When focusing the tab all servers should be pinged, (N.B. this must not block the UI thread)

Where servers are up according to systemd or status scripts but down according to pings
They should be shown as "zombie" servers 🧟

## Arse - automatic recovery of server execution.

Often, during development, new servers are created that are not yet 100% functional.
As well as failing to start, servers may fail to **stop** cleanly, and that can leave ports blocked and other long things.

An important part of server management during development is killing servers so that new ones may start. 

This should be configured in the same config file with a dedicate section for ps greps, and kills.

`[arse]`
arse_0=🧻 services
arse_check_0=ps -efwww | grep -v grep | grep-E 'nginx:|mock'
arse_kill_0=ps -efwww | grep -v grep | grep-E 'nginx:|mock' | cut -c 10-18 | xargs kill -9

# UI

UI should be rows in the same table as services.

- status - show yellow dot if the greps indicate servers are running, this may not be an error, it tests are running
- actions - buttons to kill by pids

# TODO

function to select and add servers from current systed setup
function to add local lxc servers (problem needs root to justsee them)

