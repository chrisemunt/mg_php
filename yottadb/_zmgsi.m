%zmgsi ;(CM) Service Integration - Core Server
 ;
 ;  ----------------------------------------------------------------------------
 ;  | %zmgsi                                                                    |
 ;  | Author: Chris Munt cmunt@mgateway.com, chris.e.munt@gmail.com            |
 ;  | Copyright (c) 2016-2020 M/Gateway Developments Ltd,                      |
 ;  | Surrey UK.                                                               |
 ;  | All rights reserved.                                                     |
 ;  |                                                                          |
 ;  | http://www.mgateway.com                                                  |
 ;  |                                                                          |
 ;  | Licensed under the Apache License, Version 2.0 (the "License"); you may  |
 ;  | not use this file except in compliance with the License.                 |
 ;  | You may obtain a copy of the License at                                  |
 ;  |                                                                          |
 ;  | http://www.apache.org/licenses/LICENSE-2.0                               |
 ;  |                                                                          |
 ;  | Unless required by applicable law or agreed to in writing, software      |
 ;  | distributed under the License is distributed on an "AS IS" BASIS,        |
 ;  | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. |
 ;  | See the License for the specific language governing permissions and      |
 ;  | limitations under the License.                                           |
 ;  ----------------------------------------------------------------------------
 ;
a0 d vers^%zmgsis
 q
 ;
start(port) ; start daemon
 new $ztrap set $ztrap="zgoto "_$zlevel_":starte^%zmgsi"
 s port=+$g(port)
 k ^%zmgsi("stop"),^%zmgsi("tcp_port",port)
 ; non-concurrent tcp service (old mumps systems)
 j m($g(port))
 q
starte ; error
 q
 ;
eeestart ; start
 d start(0)
 q
 ;
stop(port) ; stop
 w !,"stopping mgsi ... "
 s pport=+$g(port)
 d stop1(pport,1)
 w !!,"mgsi stopped",!
 q
 ;
stop1(port,context) ; stop daemon
 ; context==0: stop child processes
 ; context==1: stop master and child processes
 s pport=+$g(port)
 i pport d stop2(pport,context) q
 s pport="" f  s pport=$o(^%zmgsi("tcp_port",pport)) q:pport=""  d stop2(pport,context)
 q
 ;
stop2(pport,context) ; stop series
 s cport="" f  s cport=$o(^%zmgsi("tcp_port",pport,cport)) q:cport=""  d
 . s pid=$g(^%zmgsi("tcp_port",pport,cport))
 . d killproc(pid)
 . k ^%zmgsi("tcp_port",pport,cport)
 . q
 i context=1 s pid=$g(^%zmgsi("tcp_port",pport)) d killproc(pid) k ^%zmgsi("tcp_port",pport)
 q
 ;
killproc(pid) ; stop this listener
 i '$l(pid) q
 w !,"stop: "_pid
 zsy "kill -term "_pid
 q
 ;
m(port)  ; non-concurrent tcp service (old mumps systems)
 new $ztrap set $ztrap="zgoto "_$zlevel_":mh^%zmgsi"
 d seterror^%zmgsis("")
 s dev=""
 s port=+$g(port)
 i 'port s port=7041
 ; initialize list of 'used' tcp server ports
 k ^%zmgsi("tcp_port",port)
 s ^%zmgsi("tcp_port",port)=$j
 ;
ma ; set tcp server device
 set dev="server$"_$j,timeout=30 
 ;
 ; open tcp server device
 open dev:(zlisten=port_":tcp":attach="server"):timeout:"socket"
 ;
 ; use tcp server device
 use dev 
 write /listen(1) 
 ;
 set %znsock="",%znfrom=""
 s ok=1 f  d  q:ok  i $d(^%zmgsi("stop")) s ok=0 k ^%zmgsi("stop") q
 . write /wait(timeout)
 . i $key'="" s ok=1 q
 . s ok=0
 . q
 i 'ok g mx
 set %znsock=$piece($key,"|",2),%znfrom=$piece($key,"|",3)
 d event^%zmgsis("incoming connection from "_%znfrom_", starting child server process ("_%znsock_")")
 ;
 ; d child^%zmgsis(port,port,1,"")
 ;
 s errors=0
 d vars^%zmgsis
m1 ; read the next command from the mgsi gateway
 new $ztrap set $ztrap="zgoto "_$zlevel_":me^%zmgsi"
 d seterror^%zmgsis("")
 s req="" f i=1:1 r *x q:x=10!(x=0)  i i<300 s req=req_$c(x)
 ;d event^%zmgsis("command::"_(req[$c(10))_":"_req)
 i x=0 c dev g ma
 s errors=0
 s cmnd=$p(req,"^",2)
 ;
 ; only interested in the request to start a new service job
 ; and the close-down command - discard everything else
 k res
 i cmnd="s" d dint ; start a new service job
 i cmnd="x" g mx ; close-down this service
 ;
 ; flush output buffer
 d end^%zmgsis
 c dev g ma
me ; error - probably client disconnect (which is normal)
 new $ztrap set $ztrap="zgoto "_$zlevel_":mx^%zmgsi"
 s errors=errors+1 i errors<37 c dev g ma
 ; too many errors
 d event^%zmgsis("accept loop - too many errors - closing down ("_$$error^%zmgsis()_")")
mx ; exit
 new $ztrap set $ztrap="zgoto "_$zlevel_":mh^%zmgsi"
 d event^%zmgsis("closing server")
 ;
 ; close tcp server device
 c dev
 h
 ;
mh ; start-up error - halt
 new $ztrap set $ztrap="zgoto "_$zlevel_":mh1^%zmgsi"
 d event^%zmgsis("service start-up error: "_$$error^%zmgsis())
 ;
 ; close tcp server device
 i $l($g(dev)) c dev
 h
mh1 ; halt
 h
 ;
dint ; start-up and initialise a new service job
 n %uci,%touci,systype,cport,dev,i,txt,timeout,x
 s %touci=$p($p(req,"uci=",2),"&",1) ; uci for service job
 s %uci=$$getuci^%zmgsis() ; this uci (should be manager)
 s systype=$$getsys^%zmgsis() ; system type
 ;
 ; get the next available tcp port for server child process
 f cport=port+1:1 i '$d(^%zmgsi("tcp_port",port,cport)),'$d(^%zmgsi("tcp_port_excluded",cport)) q
 ; mark the port as in-use
 s ^%zmgsi("tcp_port",port,cport)=""
 ;
 ; start server child process
 new $ztrap set $ztrap="zgoto "_$zlevel_":dinte^%zmgsi"
 s ok=1,timeout=10
 j child^%zmgsis(port,cport,0,%touci)
 s ^%zewd("mgsis",$zjob)=""
 i 'ok g dinte
 ; send confirmation of a successful child start-up to the mgsi gateway 
 s txt="pid="_$j_"&uci="_%uci_"&server_type="_systype_"&version="_$p($$v^%zmgsis(),".",1,3)_"&child_port="_cport
 k res s res="" s res(1)="00000cv"_$c(10),maxlen=$$getslen^%zmgsis() d send^%zmgsis(txt)
 q
dinte ; probably a namespace/uci error
 s x="" f  s x=$o(^%zmgsi("tcp_port",x)) q:x=""  k ^%zmgsi("tcp_port",x,cport)
 d event^%zmgsis("unable to start a child process: "_$$error^%zmgsis())
 s txt="error# "_$$error^%zmgsis()
 k res s res="" s res(1)="00000cv"_$c(10),maxlen=$$getslen^%zmgsis() d send^%zmgsis(txt)
 q
 ;
ylink ; link all routines
 ;;zlink "_zmgsi.m"
 zlink "_zmgsis.m"
 q
 ;
