#! /bin/sh
# /etc/init.d/loraflow.sh

### BEGIN INIT INFO
# Provides:          loraflow.py
# Required-Start:    $remote_fs $syslog mysql
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
### END INIT INFO

# Carry out specific functions when asked to by the system
case "$1" in
  start)
    echo "Starting loraflow.py"
    /usr/local/bin/loraflow.py &
    ;;
  stop)
    echo "Stopping loraflow.py"
    pkill -f /usr/local/bin/loraflow.py
    ;;
  *)
    echo "Usage: /etc/init.d/loraflow.sh {start|stop}"
    exit 1
    ;;
esac

exit 0
