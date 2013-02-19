import httplib
import urllib2
import re
from poster.encode import multipart_encode
from poster.streaminghttp import register_openers
import sys
import BeautifulSoup
register_openers()

datagen, headers = multipart_encode({'login_user':sys.argv[1],'password':sys.argv[2],"subm_file": open(sys.argv[3], "rb"),'lang':sys.argv[4],'problemcode':sys.argv[5]})

# test.py is the file
# lang = 4 means Python 2.7
# lang = 11 means gcc 4.3.2

url="http://www.spoj.com/submit/complete/"

request = urllib2.Request(url, datagen, headers)
m = re.search(r'"newSubmissionId" value="(\d+)"/>', urllib2.urlopen(request).read())
submid=m.group(1)
submid=str(submid)
stats="statusres_"+submid
memory="statusmem_"+submid
time="statustime_"+submid
data=urllib2.urlopen("https://www.spoj.com/status/").read()
new=open("ser.html","w")
new.write(data)
new.close()
soup = BeautifulSoup.BeautifulSoup(data)
tim=soup.find("td", {"id":time})
mem=soup.find("td",{"id":memory})
sta=soup.find("td",{"id":stats})
final="Submission id: "+submid+"Status: "+str(sta)+"Time: "+str(tim)+"Memory: "+str(mem)
soup = BeautifulSoup.BeautifulSoup(final)
gy=''
for node in soup.findAll('td'):
    gy+=''.join(node.findAll(text=True))
y=str(gy)
y=y.replace("google_ad_section_start(weight=ignore) ","")
y=y.replace("google_ad_section_end","")
y=y.replace("\n","")
y=y.replace("  		","")
y=y.replace("  		    	","")
print y
#for node in soup.findAll('a'):
#    print ''.join(node.findAll(text=True)) 
#print final
# The request has been submitted"""
