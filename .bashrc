# .bashrc

# Source global definitions
if [ -f /etc/bashrc ]; then
	. /etc/bashrc
fi

# User specific aliases and functions
export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/import/tools/qa/bin:/import/lab/bin:.:$PATH


killscreens () {
        screen -ls | grep Detached | cut -d. -f1 | awk '{print $1}' | xargs kill
        echo "------------no running screen sessions now-----------"
        screen -list

}

function freem {
    free -m | awk 'NR==3 {print $4 " MB"}' 
}
