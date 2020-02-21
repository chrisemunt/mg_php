%zmgsis ;(CM) Service Integration - Child Process
 ;
 ;  ----------------------------------------------------------------------------
 ;  | %zmgsis                                                                  |
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
a0 d vers q
 ;
 ; v2.0.6:   17 February  2009
 ; v2.0.7:    1 July      2009
 ; v2.0.8:   15 July      2009
 ; v2.0.9:   19 October   2009
 ; v2.1.10:  12 August    2013
 ; v2.2.11:   9 December  2013
 ; v2.3.12:   6 September 2018 (Add Intersystems IRIS)
 ; v2.3.13:   3 February  2019 (Add all-out to xdbc)
 ; v2.3.14:  18 March     2019 (Release as Open Source, Apache v2 license)
 ; v3.0.1:   13 June      2019 (Renamed to %zmgsi and %zmgsis)
 ; v3.1.2:   10 September 2019 (add protocol upgrade for mg_dba library - Go)
 ; v3.2.3:    1 November  2019 (add SQL interface)
 ; v3.2.4:    8 January   2020 (Add support for $increment to old protocol)
 ; v3.2.5:   17 January   2020 (Finalise the ifc interface)
 ;
v() ; version and date
 n v,r,d
 s v="3.2"
 s r=5
 s d="17 January 2020"
 q v_"."_r_"."_d
 ;
vers ; version information
 n v
 s v=$$v()
 w !,"M/Gateway Developments Ltd - Service Integration Gateway"
 w !,"Version: "_$p(v,".",1,2)_"; Revision "_$p(v,".",3)_" ("_$p(v,".",4)_")"
 w !
 q
 ;
isydb() ; see if this is yottadb
 i $zv["GT.M" q 1
 q 0
 ;
isidb() ; see if this is an intersystems database
 i $zv["ISM" q 1
 i $zv["Cache" q 2
 i $zv["IRIS" q 3
 q 0
 ;
ismsm() ; see if this is msm
 i $zv["MSM" q 1
 q 0
 ;
isdsm() ; see if this is dsm
 i $zv["DSM" q 1
 q 0
 ;
ism21() ; see if this is m21
 i $zv["M21" q 1
 q 0
 ;
vars ; public  system variables
 ;
 ; the following variables can be modified in accordance with the documentation
 s extra=$c(1) ; key marker for oversize data strings
 s abyref=0 ; set to 1 to treat all arrays as if they were passed by reference
 s mqinfo=0 ; set to 1 to place all mq error/information messages in %zmgmq("info")
 ;            otherwise, error messages will be placed in %zmgmq("error")
 ;            and 'information only' messages in %zmgmq("info")
 ;
 ; the following variables must not be modified
 i '($d(global)#10) s global=0
 i '($d(oversize)#10) s oversize=0
 i '($d(offset)#10) s offset=0
 i '($d(version)#10) s version=+$$v()
 ; #define mg_tx_data     0
 ; #define mg_tx_akey     1
 ; #define mg_tx_arec     2
 ; #define mg_tx_eod      3
 s ddata=0,dakey=1,darec=2,deod=3
 q
 ;
esize(esize,size,base)
 n i,x
 i base'=10 g esize1
 s esize=size
 q $l(esize)
esize1 ; up to base 62
 s esize=$$ebase62(size#base)
 f i=1:1 s x=(size\(base**i)) q:'x  s esize=$$ebase62(x#base)_esize
 q $l(esize)
 ;
dsize(esize,len,base)
 n i,x
 i base'=10 g dsize1
 s size=+$e(esize,1,len)
 q size
dsize1 ; up to base 62
 s size=0
 f i=len:-1:1 s x=$e(esize,i) s size=size+($$dbase62(x)*(base**(len-i)))
 q size
 ;
ebase62(n10) ; encode to single digit (up to) base-62 number
 i n10'<0,n10<10 q $c(48+n10)
 i n10'<10,n10<36 q $c(65+(n10-10))
 i n10'<36,n10<62 q $c(97+(n10-36))
 q ""
 ;
dbase62(nxx) ; decode single digit (up to) base-62 number
 n x
 s x=$a(nxx)
 i x'<48,x<58 q (x-48)
 i x'<65,x<91 q ((x-65)+10)
 i x'<97,x<123 q ((x-97)+36)
 q ""
 ;
esize256(dsize) ; create little-endian 32-bit unsigned integer from M decimal
 q $c(dsize#256)_$c(((dsize\256)#256))_$c(((dsize\(256**2))#256))_$c(((dsize\(256**3))#256))
 n esize
 s esize=+dsize f  q:$l(esize)=4  s esize="0"_esize
 q esize
 ;
dsize256(esize) ; convert little-endian 32-bit unsigned integer to M decimal
 q ($a(esize,4)*(256**3))+($a(esize,3)*(256**2))+($a(esize,2)*256)+$a(esize,1)
 s dsize=+esize
 q dsize
 ;
ehead(head,size,byref,type)
 n slen,hlen
 s slen=$$esize(.esize,size,10)
 s code=slen+(type*8)+(byref*64)
 s head=$c(code)_esize
 s hlen=slen+1
 q hlen
 ;
dhead(head,size,byref,type)
 n slen,hlen,code
 s code=$a(head,1)
 s byref=code\64
 s type=(code#64)\8
 s slen=code#8
 s hlen=slen+1
 s size=0 i $l(head)'<hlen s size=$$dsize($e(head,2,slen+1),slen,10)
 q hlen
 ;
wsehead(size,type)
 n slen,esize
 s slen=$$esize(.esize,size,62)
 f  q:slen'<4  s esize="0"_esize,slen=slen+1
 s head=esize_type
 q head
 ;
wsdhead(head,type)
 s type=$e(head,5)
 s size=$$dsize(head,4,62)
 q size
 ;
rdxx(len) ; read 'len' bytes from mgsi
 n x,nmax,n,ncnt
 i 'len q ""
 s x="",nmax=len,n=0,ncnt=0 f  r y#nmax d  q:'nmax  i ncnt>100 q
 . i y="" s ncnt=ncnt+1 q
 . s ncnt=0,x=x_y,n=n+$l(y),nmax=len-n
 . q
 i ncnt s x="" d halt ; client disconnect
 q x
 ;
rdx(len,clen,rlen) ; read from mgsi - initialize: (rdxsize,rdxptr,rdxrlen)=0,rdxbuf="",maxlen=$$getslen()
 n result,get,avail
 ;
 i $d(%zcs("ifc")) s result=$e(request,%zcs("ifc"),%zcs("ifc")+(len-1)),%zcs("ifc")=%zcs("ifc")+len,rlen=rlen+len q result
 ;
 ;s result="" f get=1:1:len r *x s result=result_$c(x)
 ;s rlen=rlen+len q result
 ;
 s get=len,result=""
 i 'len q result
 f  d  i 'get q
 . s avail=rdxsize-rdxptr
 . ;d event("i="_i_";len="_len_";avail="_avail_";get="_get_"=("_rdxbuf_") "_clen_" "_rlen)
 . i get'>avail s result=result_$e(rdxbuf,rdxptr+1,rdxptr+get),rdxptr=rdxptr+get,get=0 q
 . ;s result=rdxbuf,rdxptr=0,get=get-avail
 . s result=$e(rdxbuf,rdxptr+1,rdxptr+avail),rdxptr=0,get=get-avail
 . s avail=clen-rdxrlen i 'avail q
 . i avail>maxlen s avail=maxlen 
 . s rdxbuf=$$rdxx(avail),rdxsize=avail,rdxptr=0,rdxrlen=rdxrlen+avail
 . ;d event("rdxbuf="_i_"="_rdxbuf)
 . q
 s rlen=rlen+len
 q result
 ;
inetd ; entry point from [x]inetd
xinetd ; someone is sure to use this label
 new $ztrap set $ztrap="zgoto "_$zlevel_":inetde^%zmgsis"
 d child(0,0,1,"")
 q
inetde ; error
 w $$error()
 q
 ;
ifc(ctx,request,param) ; entry point from fixed binding
 n %zcs,clen,hlen,result,rlen,abyref,anybyref,argc,array,buf,byref,cmnd,dakey,darec,ddata,deod,eod,esize,extra,fun,global,hlen,maxlen,mqinfo,nato,offset,ok,oversize,pcmnd,rdxbuf,rdxptr,rdxrlen,rdxsize,ref,refn,mreq1,req,req1,req2,req3,res,size,sl,slen,sn,sysp,type,uci,var,version,x
 new $ztrap set $ztrap="zgoto "_$zlevel_":ifce^%zmgsis"
 d vars
 i param["$zv" q $zv
 s %zcs("ifc")=1
 s argc=1,array=0,nato=0
 k ^%zmg("mpc",$j),^mgsi($j)
 s maxlen=$$getslen()
 s (rdxsize,rdxptr,rdxrlen)=0,rdxbuf=""
 s sn=0,sl=0,ok=1,type=0,offset=0,var="req"_argc,req(argc)=var,(cmnd,pcmnd,buf)=""
 s buf=$p(request,$c(10),1),%zcs("ifc")=%zcs("ifc")+$l(buf)+1
 s type=0,byref=0 d req1 s @var=buf
 s cmnd=$p(buf,"^",2)
 s hlen=$l(buf),clen=0
 i cmnd="P" s clen=$$dsize($e(buf,hlen-(5-1),hlen),5,62)
 s %zcs("client")=$e(buf,4)
 ;d event("request cmnd="_cmnd_"; size="_clen_" ("_$e(buf,hlen-(5-1),hlen)_"); client="_%zcs("client")_" ;header = "_buf)
 s rlen=0
 i clen d req
 s req=$g(@req(1)) i req="" q ""
 s cmnd=$p(req,"^",2)
 k res s res="" s res(1)="00000cv"_$c(10)
 i cmnd="A" d ayt
 i cmnd="S" d dint
 i cmnd="P" d mphp
 i cmnd="H" d info
 i cmnd="X" d halt
 d end s result=$g(res)
 k res s res=""
 q result
ifce ; error
 q "00000ce"_$c(10)_"m server error : ["_$g(req(2))_"]"_$tr($$error(),"<>%","[]^")
 ;
ifct ; ifc test
 k
 s req=$$ifct1()
 s res=$$ifc(0,req,"")
 q
 ;
ifct1() ; test data
 n
 d vars
 s server="localhost",uci="",vers="1.1.1",smeth=0,cmnd="S"
 s req="PHPp^P^"_server_"#"_uci_"#0###"_vers_"#"_smeth_"^"_cmnd_"^00000"_$c(10)
 s hlen=$l(req)
 ;
 s data="cm",size=$l(data),byref=0,type=ddata
 s x=$$ehead(.head,size,byref,type)
 s req=req_head_data
 ;
 s data="1",size=$l(data),byref=0,type=ddata
 s x=$$ehead(.head,size,byref,type)
 s req=req_head_data
 ;
 s data="data",size=$l(data),byref=0,type=ddata
 s x=$$ehead(.head,size,byref,type)
 s req=req_head_data
 ;
 s x=$$esize(.esize,$l(req)-hlen,62)
 s esize=$e("00000",1,(5-x))_esize
 s req=$p(req,"00000",1)_esize_$p(req,"00000",2,9999)
 k (req)
 q req
 ;
child(pport,port,conc,uci) ; child
 new $ztrap set $ztrap="zgoto "_$zlevel_":childe^%zmgsis"
 i uci'="" d uci(uci)
 i 'conc d
 . s ^%zmgsi("tcp_port",pport,port)=$j
 . set dev="server$"_$j,timeout=30
 . ; open tcp server device
 . open dev:(zlisten=port_":tcp":attach="server"):timeout:"socket"
 . use dev
 . write /listen(1)
 . set %znsock="",%znfrom=""
 . s ok=1 f  d  q:ok  i $d(^%zmgsi("stop")) s ok=0 q
 . . write /wait(timeout)
 . . i $key'="" s ok=1 q
 . . d event^%zmgsis("write /wait(timeout) expires")
 . . s ok=0
 . . q
 . i 'ok c dev h
 . set %znsock=$piece($key,"|",2),%znfrom=$piece($key,"|",3)
 . ;d event^%zmgsis("incoming child connection from "_%znfrom_" ("_%znsock_")")
 . q
 ;
 s nato=0
child2 ; child request loop
 d vars
 k ^%zmg("mpc",$j),^mgsi($j)
 f i=1:1:37 k @("req"_i)
 k req s argc=1,array=0
 i '($d(nato)#10) s nato=0
child3 ; read request
 ;d event("******* get next request *******")
 s maxlen=$$getslen()
 s (rdxsize,rdxptr,rdxrlen)=0,rdxbuf=""
 s sn=0,sl=0,ok=1,type=0,offset=0,var="req"_argc,req(argc)=var,(cmnd,pcmnd,buf)=""
 i 'nato r *x
 i nato r *x:nato i '$t d halt ; no-activity timeout
 i x=0 d halt ; client disconnect
 s buf=$c(x) f  r *x q:x=10!(x=0)  s buf=buf_$c(x)
 i x=0 d halt ; client disconnect
 i buf="xDBC" g main^%mgsqln
 i buf?1u.e1"HTTP/"1n1"."1n1c s buf=buf_$c(10) g main^%mgsqlw
 i $e(buf,1,4)="dbx1" d dbx^%zmgsis(buf) g halt
 s type=0,byref=0 d req1 s @var=buf
 s cmnd=$p(buf,"^",2)
 s hlen=$l(buf),clen=0
 i cmnd="P" s clen=$$dsize($e(buf,hlen-(5-1),hlen),5,62)
 s %zcs("client")=$e(buf,4)
 ;d event("request cmnd="_cmnd_"; size="_clen_" ("_$e(buf,hlen-(5-1),hlen)_"); client="_%zcs("client")_" ;header = "_buf)
 s rlen=0
 i clen d req
 ;
 ;f i=1:1:argc d event("arg "_i_" = "_$g(@req(i)))
 ;
 s req=$g(@req(1)) i req="" g child2
 s cmnd=$p(req,"^",2)
 k res s res="" s res(1)="00000cv"_$c(10)
 i cmnd="A" d ayt
 i cmnd="S" d dint
 i cmnd="P" d mphp
 i cmnd="H" d info
 i cmnd="X" d halt
 d end
 k res s res=""
 g child2
 ;
childe ; error
 d event($$error())
 i $$error()["read" g halt
 i $$error()["%gtm-e-ioeof" g halt
 g child2
 ;
halt ; halt
 i 'conc d
 . ; close tcp server device
 . i $l($g(dev)) c dev
 . s x="" f  s x=$o(^%zmgsi("tcp_port",x)) q:x=""  k ^%zmgsi("tcp_port",x,port)
 . q
 h
 ;
req ; read request data
 n port,dev,conc,get,got
req0 ; get next argument
 s x=$$rdx(1,clen,.rlen),hlen=$$dhead(x,.size,.byref,.type)
 ;d event("(1) clen="_clen_";rlen="_rlen_";hlen="_hlen_";argc="_argc_";size="_size_";byref="_byref_";type="_type)
 s slen=hlen-1
 s esize=$$rdx(slen,clen,.rlen)
 s size=$$dsize(esize,slen,10)
 ;d event("(2) clen="_clen_";rlen="_rlen_";hlen="_hlen_";slen="_slen_";argc="_argc_";size="_size_";byref="_byref_";type="_type)
 s argc=argc+1
 d req1
 i type=darec d array g reqz
 s got=0 f sn=0:1 s get=size-got s:get>maxlen get=maxlen s buf=$$rdx(get,clen,.rlen) d  i got=size q
 . s got=got+get
 . ;d event("(3) data: clen="_clen_";rlen="_rlen_";size="_size_";get="_get_";sn="_sn_";pcmnd="_pcmnd_";buf="_buf)
 . i argc=3,pcmnd="h" s @var=buf d mpc q
 . i 'sn s @var=buf q
 . i sn s @(var_"(extra,sn)")=buf q
 . q
reqz ; argument read
 i rlen<clen g req0
 s eod=1
 q
 ;
req1 ; initialize next argument
 i argc=1 d
 . s cmnd=$p(buf,"^",2)
 . s sysp=$p(buf,"^",3)
 . s uci=$p(sysp,"#",2) i uci'="" d
 . . s ucic=$$getuci()
 . . i ucic'="",uci=ucic q
 . . d uci(uci) d event("correct namespace from '"_ucic_"' to '"_uci_"'")
 . . q
 . s offset=$p(sysp,"#",3)+0
 . s global=$p(sysp,"#",7)+0
 . s pcmnd=$p(buf,"^",4)
 . q
 s sn=0,sl=0
 s var="req"_argc
 s req(argc)=var
 s req(argc,0)=type i type=darec s req(argc,0)=1
 s req(argc,1)=byref
 i type=1,abyref=1 s req(argc,1)=1
 q
 ;
mpc ; raw content for http post: save section of data
 s sn=sn+1,^%zmg("mpc",$j,"content",sn)=buf,buf="",sl=0
 q
 ;
array ; read array
 n x,kn,val,sn,ext,get,got
 ;d event("*** array ***")
 k x,ext s kn=0
array0 ; read next element (key or data)
 s sn=0,sl=0
 s x=$$rdx(1,clen,.rlen),hlen=$$dhead(x,.size,.byref,.type)
 ;d event("(1) array clen="_clen_";rlen="_rlen_";hlen="_hlen_";argc="_argc_";size="_size_";byref="_byref_";type="_type)
 s slen=hlen-1
 s esize=$$rdx(slen,clen,.rlen)
 s size=$$dsize(esize,slen,10)
 ;d event("(2) array clen="_clen_";rlen="_rlen_";hlen="_hlen_";slen="_slen_";argc="_argc_";size="_size_";byref="_byref_";type="_type)
 i type=deod q
 s got=0 f sn=0:1 s get=size-got s:get>maxlen get=maxlen s buf=$$rdx(get,clen,.rlen) d  i got=size q
 . s got=got+get
 . ;d event("(3) array data clen="_clen_";rlen="_rlen_";size="_size_";get="_get_";sn="_sn_";pcmnd="_pcmnd_";buf="_buf)
 . i argc=3,pcmnd="h" s @var=buf d mpc q
 . i type=dakey s kn=kn+1,x(kn)=buf 
 . i type=ddata s val=buf d array1 k x,ext s kn=0
 . q
 g array0
 ;
array1 ; read array - set a single node
 n n,i,ref,com,key
 new $ztrap set $ztrap="zgoto "_$zlevel_":array1e^%zmgsis"
 s (key,com)="" f i=1:1:kn q:i=kn&($g(x(i))=" ")  s key=key_com_"x("_i_")",com=","
 i global d
 . i $l(key) s ref="^mgsi($j,argc-2,"_key_")",eref="^mgsi($j,argc-2,"_key_",extra,sn)"
 . i '$l(key) s ref="^mgsi($j,argc-2)",eref="^mgsi($j,argc-2,extra,sn)"
 . q
 i 'global d
 . i $l(key) s ref=req(argc)_"("_key_")",eref=req(argc)_"("_key_",extra,sn)"
 . i '$l(key) s ref=req(argc),eref=req(argc)_"(extra,sn)"
 . q
 i $l(ref) x "s "_ref_"=val"
 f sn=1:1 q:'$d(ext(sn))  x "s "_eref_"=ext(sn)"
 q
array1e ;
 d event("array: "_$$error())
 q
 ;
end ; terminate response
 n len,len62,i,head,x
 i '$d(%zcs("ifc")),$e($g(res(1)),6,7)="sc" w $p(res(1),"0",1) d flush q  ; streamed response
 s len=0
 f i=1:1 q:'$d(res(i))  s len=len+$l(res(i))
 s len=len-8
 s head=$e($g(res(1)),1,8)
 s x=$$esize(.len62,len,62)
 f  q:$l(len62)'<5  s len62="0"_len62
 s head=len62_$e(head,6,8) i $l(head)'=8 s head=len62_"cv"_$c(10)
 s res(1)=head_$e($g(res(1)),9,99999)
 ; flush the lot out
 i $d(%zcs("ifc")) g end1
 f i=1:1 q:'$d(res(i))  w res(i)
 d flush
 q
end1 ; interface
 s res="" f i=1:1 q:'$d(res(i))  s res=res_res(i)
 q
 ;
flush ; flush output buffer
 q
 ;
ayt ; are you there?
 s req=$g(@req(1))
 s txt=$p($h,",",2)
 f  q:$l(txt)'<5  s txt="0"_txt
 s txt="m"_txt
 f  q:$l(txt)'<12  s txt=txt_"0"
 d send(txt)
 q
 ; 
dint ; initialise the service link
 n port,%uci,username,password,usrchk
 s (username,password)=""
 s req=$p($g(@req(1)),"^s^",2,9999)
 ;"^s^version=%s&timeout=%d&nls=%s&uci=%s"
 s username=$p(req,"&unpw=",2,999)
 s password=$p(username,$c(1),2),username=$p(username,$c(1),1)
 s usrchk=1
 s usrchk=$$checkunpw(password)
 i 'usrchk g halt
 ;d event(username_"|"_password)
 s version=$p($p(req,"version=",2),"&",1)
 s nato=+$p($p(req,"timeout=",2),"&",1)
 s %zcs("nls_trans")=$p($p(req,"nls=",2),"&",1)
 s %uci=$p($p(req,"uci=",2),"&",1)
 i $l(%uci) d uci(%uci)
 s x=$$setio(%zcs("nls_trans"))
 s %uci=$$getuci()
 s systype=$$getsys()
 s txt="pid="_$j_"&uci="_%uci_"&server_type="_systype_"&version="_$p($$v(),".",1,3)_"&child_port=0"
 d send(txt)
 q
 ;
uci(uci) ; change namespace/uci
 n port,dev,conc
 new $ztrap set $ztrap="zgoto "_$zlevel_":ucie^%zmgsis"
 i uci="" q
 s $zg=uci
 q
ucie ; error
 d event("uci error: "_uci_" : "_$$error())
 q
 ;
info ; connection info
 n port,dev,conc,nato
 d send("HTTP/1.1 200 OK"_$char(13,10)_"Connection: close"_$char(13,10)_"Content-Type: text/html"_$char(13,10,13,10))
 d send("<html><head><title>mgsi - connection test</title></head><body bgcolor=#ffffcc>")
 d send("<h2>mgsi - connection test successful</h2>")
 d send("<table border=1>")
 d send("<tr><td>"_$$getsys()_" version:</td><td><b>"_$zv_"<b><tr>")
 d send("<tr><td>uci:</td><td><b>"_$$getuci()_"<b><tr>")
 d send("</table>")
 d send("</body></html>")
 q
 ;
event(text) ; log m-side event
 n port,dev,conc,n,emax
 new $ztrap set $ztrap="zgoto "_$zlevel_":evente^%zmgsis"
 f i=1:1 s x=$e(text,i) q:x=""  s y=$s(x=$c(13):"\r",x=$c(10):"\n",1:"") i y'="" s $e(text,i)=y
 s emax=100 ; maximum log size (no. messages)
 l +^%zmgsi("log")
 s n=$g(^%zmgsi("log")) i n="" s n=0
 s n=n+1,^%zmgsi("log")=n
 l -^%zmgsi("log")
 s ^%zmgsi("log",n,0)=$$head(),^%zmgsi("log",n,1)=text
 f n=n-emax:-1 q:'$d(^%zmgsi("log",n))  k ^(n)
 q
evente ; error
 q
 ;
ddate(date) ; decode m date
 new $ztrap set $ztrap="zgoto "_$zlevel_":ddatee^%zmgsis"
 q $zd(date,2)
ddatee ; no $zd function
 q date
 ;
dtime(time) ; decode m time
 q (time\3600)_":"_(time#3600\60)
 ;
head() ; format header record
 n %uci
 new $ztrap set $ztrap="zgoto "_$zlevel_":heade^%zmgsis"
 s %uci=$$getuci()
heade ; error
 q $$ddate(+$h)_" at "_$$dtime($p($h,",",2))_"~"_$g(%zcs("port"))_"~"_%uci
 ;
hmacsha256(string,key,b64,context) ; hmac-sha256
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"hmac-sha256",string,key,b64,context)
 ;
hmacsha1(string,key,b64,context) ; hmac-sha1
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"hmac-sha1",string,key,b64,context)
 ;
hmacsha(string,key,b64,context) ; hmac-sha
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"hmac-sha",string,key,b64,context)
 ;
hmacmd5(string,key,b64,context) ; hmac-md5
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"hmac-md5",string,key,b64,context)
 ;
sha256(string,b64,context) ; sha256
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"sha256",string,"",b64,context)
 ;
sha1(string,b64,context) ; sha1
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"sha1",string,"",b64,context)
 ;
sha(string,b64,context) ; sha
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"sha",string,"",b64,context)
 ;
md5(string,b64,context) ; md5
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"md5",string,"",b64,context)
 ;
b64(string,context) ; base64
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"b64",string,"",0,context)
 ;
db64(string,context) ; decode base64
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"d-b64",string,"",0,context)
 ;
time(context) ; time
 q $$crypt("127.0.0.1",$s(context:80,1:7040),"time","","",0,context)
 ;
zts(context) ; zts
 s time=$$time(context)
 s zts=$p($h,",",1)_","_(($p(time,":",1)*60*60)+($p(time,":",2)*60)+$p(time,":",3))
 q zts
 ;
crypt(ip,port,method,string,key,b64,context)
 n %zmgmq,response,method
 s method=method i b64 s method=method_"-b64"
 s %zmgmq("send")=string
 s %zmgmq("key")=key
 i context=0 s response=$$wsmq(ip,port,method,.%zmgmq)
 i context=1 s response=$$wsx(ip,port,method,.%zmgmq)
 q $g(%zmgmq("recv"))
 ;
wsmq(ip,port,request,%zmgmq) ; message for websphere mq (parameters passed by reference)
 q $$wsmq1(ip,port,request)
 ;
wsmq1(ip,port,request) ; message for websphere mq (parameters passed by global array - %zmgmq())
 n (ip,port,request,%zmgmq)
 new $ztrap set $ztrap="zgoto "_$zlevel_":wsmqe^%zmgsis"
 ;
 ; close connection to gateway
 i request="close" d  s result=1 g wsmq1x
 . i $d(%zmgmq("keepalive","dev")) s dev=%zmgmq("keepalive","dev") k %zmgmq("keepalive") c dev
 . q
 ;
 d vars
 s crlf=$c(13,10)
 s request=$$ucase(request)
 i request="get" k %zmgsi("send")
 s res=""
 s %zmgmq("error")=""
 s global=+$g(%zmgmq("global"))
 ;
 ; create tcp connection to gateway
 s ok=1
 i $d(%zmgmq("keepalive","dev")) s dev=%zmgmq("keepalive","dev")
 i '$d(%zmgmq("keepalive","dev")) d
 . s dev="client$"_$j,timeout=10
 . o dev:(connect=ip_":"_port_":tcp":attach="client"):timeout:"socket" e  s %zmgmq("error")="cannot connect to mgsi" s ok=0
 . q
 i 'ok s result=0 g wsmq1x
 ;
 s maxlen=$$getslen()
 s buffer="",bsize=0,eof=0
 s dev(0)=$s($$isidb()!$$isydb():$io,1:0)
 u dev
 s req="wsmq "_request_" v1.1"_crlf d wsmqs
 s x="" f  s x=$o(%zmgmq(x)) q:x=""  i x'="recv",x'="send" s y=$g(%zmgmq(x)) i y'="" s req=x_": "_y_crlf d wsmqs
 s req=crlf d wsmqs
 i global d
 . s req=$g(^mgsi($j,1,"send")) i req'="" d wsmqs
 . s x="" f  s x=$o(^mgsi($j,1,"send",extra,x)) q:x=""  s req=$g(^mgsi($j,1,"send",extra,x)) i req'="" d wsmqs
 . s x="" f  s x=$o(^mgsi($j,1,"send",x)) q:x=""  i x'=extra s req=$g(^mgsi($j,1,"send",x)) i req'="" d wsmqs
 . q
 i 'global d
 . s req=$g(%zmgmq("send")) i req'="" d wsmqs
 . s x="" f  s x=$o(%zmgmq("send",extra,x)) q:x=""  s req=$g(%zmgmq("send",extra,x)) i req'="" d wsmqs
 . s x="" f  s x=$o(%zmgmq("send",x)) q:x=""  i x'=extra s req=$g(%zmgmq("send",x)) i req'="" d wsmqs
 . q
 ;s req=$c(deod),eof=1 d wsmqs
 s req=$c(7),eof=1 d wsmqs
 u dev
 s size=+$$rdxx(10)
 s res="",len=0,sn=0,got=0,pre="",plen=0,hdr=1 f  d  q:got=size
 . s get=size-got i get>maxlen s get=maxlen
 . s x=$$rdxx(get) s got=got+get
 . i got=size,$e(x,get)=$c(deod) s x=$e(x,1,get-1)
 . i hdr d  q
 . . s lx=$l(x,$c(13)) f i=1:1:lx d  q:'hdr
 . . . s r=$p(x,$c(13),i)
 . . . i i=lx s pre=r,plen=$l(pre) q
 . . . i plen s r=pre_r,pre="",plen=0
 . . . i r=$c(10) s hdr=0 s pre=$p(x,$c(13),i+1,99999) s:$e(pre)=$c(10) pre=$e(pre,2,99999) s plen=$l(pre) q
 . . . s nam=$p(r,": ",1),val=$p(r,": ",2,99999)
 . . . i $e(nam)=$c(10) s nam=$e(nam,2,99999)
 . . . i nam'="" s %zmgmq(nam)=val
 . . . q
 . . q
 . s to=maxlen-plen
 . s res=pre_$e(x,1,to),pre=$e(x,to+1,99999),plen=$l(pre)
 . i global d
 . . i 'sn s ^mgsi($j,1,"recv")=res,res="",sn=sn+1 q
 . . i sn s ^mgsi($j,1,"recv",extra,sn)=res,res="",sn=sn+1 q
 . . q
 . i 'global d
 . . i 'sn s %zmgmq("recv")=res,res="",sn=sn+1 q
 . . i sn s %zmgmq("recv",extra,sn)=res,res="",sn=sn+1 q
 . . q
 . q
 s result=1
 i global d
 . i plen,'sn s ^mgsi($j,1,"recv")=pre,plen=0,sn=sn+1
 . i plen,sn s ^mgsi($j,1,"recv",extra,sn)=pre,plen=0,sn=sn+1
 . i $g(^mgsi($j,1,"r_code"))=2033 s result=0
 . i $l($g(^mgsi($j,1,"error"))) s result=0
 . i $g(^mgsi($j,1,"recv"))[":mgsi:error:" s result=0
 . q
 i 'global d
 . i plen,'sn s %zmgmq("recv")=pre,plen=0,sn=sn+1
 . i plen,sn s %zmgmq("recv",extra,sn)=pre,plen=0,sn=sn+1
 . i $g(%zmgmq("r_code"))=2033 s result=0
 . i $l($g(%zmgmq("error"))) s result=0
 . i $g(%zmgmq("recv"))[":mgsi:error:" s result=0
 . q
 u dev(0)
 i $g(%zmgmq("keepalive"))=1 s %zmgmq("keepalive","dev")=dev
 i $g(%zmgmq("keepalive"))'=1 c dev
wsmq1x ; exit point
 i $g(mqinfo) m %zmgmq("info")=%zmgmq("error") s %zmgmq("error")=""
 q result
 ;
wsmqe ; error (eof)
 new $ztrap set $ztrap="zgoto "_$zlevel_":"
 i $d(dev(0)) u dev(0)
 i '$d(dev(0)) u 0
 i $g(%zmgmq("keepalive"))=1 s %zmgmq("keepalive","dev")=dev
 i $g(%zmgmq("keepalive"))'=1 c dev
 i $l($g(%zmgmq("error"))) g wsmqex
 s %zmgmq("error")=$$error() g wsmqex
wsmqex ; exit point
 i $g(mqinfo) m %zmgmq("info")=%zmgmq("error") s %zmgmq("error")=""
 q 0
 ;
wsmqs ; send outgoing header
 n n,x,len
wsmqs1 s len=$l(req)
 i (bsize+len)<maxlen s buffer=buffer_req,bsize=bsize+len,req="",len=0 i 'eof q
 i $$isdsm() w buffer d flush
 s buffer=req,bsize=len
 i eof s req="" i bsize g wsmqs1
 q
 ;
wsmqsrv(%zmgmq) ; server to websphere mq
 new $ztrap set $ztrap="zgoto "_$zlevel_":wsmqsrve^%zmgsis"
 i global d
 . n x
 . s x="" f  s x=$o(^mgsi($j,1,x)) q:x=""  i x'="recv" s %zmgmq(x)=$g(^mgsi($j,1,x))
 . q
 d @$g(%zmgmq("routine"))
 k %zmgmq("qm_name")
 k %zmgmq("q_name")
 k %zmgmq("recv")
 i global d
 . k ^mgsi($j,1,"qm_name")
 . k ^mgsi($j,1,"q_name")
 . k ^mgsi($j,1,"recv")
 . m ^mgsi($j,1)=%zmgmq
 . q
 q 1
wsmqsrve ; error
 k %zmgmq("qm_name")
 k %zmgmq("q_name")
 k %zmgmq("recv")
 s %zmgmq("send")="error: "_$$error()
 s %zmgmq("error")="error: "_$$error()
 q 0
 ;
wsx(ip,port,request,%zmgmq) ; message for web server (parameters passed by reference)
 q $$wsx1(ip,port,request)
 ;
wsx1(ip,port,request) ; message for web server (parameters passed by global array - %zmgmq())
 n (ip,port,request,%zmgmq)
 new $ztrap set $ztrap="zgoto "_$zlevel_":wsxe^%zmgsis"
 d vars
 s crlf=$c(13,10)
 ;
 ; create tcp connection to server
 s ok=1
 i $d(%zmgmq("keepalive","dev")) s dev=%zmgmq("keepalive","dev")
 i '$d(%zmgmq("keepalive","dev")) d
 . s dev="client$"_$j,timeout=10
 . o dev:(connect=ip_":"_port_":tcp":attach="client"):timeout:"socket" e  s %zmgmq("error")="cannot connect to mgsi" s ok=0
 . q
 i 'ok s result=0 g wsxex
 ;
 s maxlen=$$getslen()
 s dev(0)=$s($$isidb()!$$isydb():$io,1:0)
 u dev
 s req=""
 s req=req_"POST /mgsi/sys/system_functions.mgsi?fun="_request_"&key="_$$wsxesc($g(%zmgmq("key")))_" HTTP/1.1"_crlf
 s req=req_"Host: "_ip_$s(port'=80:":"_port,1:"")_crlf
 s req=req_"Content-Type: text/plain"_crlf
 s req=req_"User-Agent: zmgsi v"_$g(version)_crlf
 s req=req_"Content-Length: "_$l($g(%zmgmq("send")))_crlf
 s req=req_"Connection: close"_crlf
 s req=req_crlf
 s req=req_$g(%zmgmq("send"))
 ;
 i $$isdsm() w buffer d flush
 ;
 s res=$$rdxx(60) f  q:res[$c(13,10,13,10)  s res=res_$$rdxx(1)
 s head=$p(res,$c(13,10,13,10),1),res=$p(res,$c(13,10,13,10),2,9999)
 s headn=$$lcase(head)
 s clen=+$p(headn,"content-length: ",2)
 s rlen=clen-$l(res)
 s res=res_$$rdxx(rlen)
wsxe ; error (eof)
 new $ztrap set $ztrap="zgoto "_$zlevel_":"
 i $d(dev(0)) u dev(0)
 i '$d(dev(0)) u 0
 c dev
wsxex ; exit point
 s %zmgmq("recv")=$g(res)
 q 0
 ;
wsxesc(in)
 n out,i,a,c,len,n16
 s out="",n16=0 f i=1:1:$l(in) d
 . s c=$e(in,i),a=$a(c)
 . i a=32 s c="+" s out=out_c q
 . i a<32!(a>127)!(c?1p) s len=$$esize(.n16,a,16) s:len=1 n16="0"_n16 s c="%"_n16 s out=out_c q
 . s out=out_c q
 . q
 q out
 ;
mphp ; request from m_client
 n port,dev,conc,cmnd,nato
 s cmnd=$p(req,"^",4)
 d php
 q
 ;
php ; serve request from m_client
 n argz,i,m
 new $ztrap set $ztrap="zgoto "_$zlevel_":phpe^%zmgsis"
 s res=""
 i cmnd="S" d set
 i cmnd="G" d get
 i cmnd="K" d kill
 i cmnd="D" d data
 i cmnd="O" d order
 i cmnd="P" d previous
 i cmnd="I" d increment
 i cmnd="M" d mergedb
 i cmnd="m" d mergephp
 i cmnd="H" d html
 i cmnd="y" d htmlm
 i cmnd="h" d http
 i cmnd="X" d proc
 i cmnd="x" d meth
 i cmnd="W" d web
 q
phpe ; error
 d event($$client()_" error : "_$$error())
 k res s res="",res(2)="m server error : ["_$g(req(2))_"]"_$tr($$error(),"<>%","[]^")_$g(ref)
 s res(1)="00000ce"_$c(10)
 q
 ;
client() ; get client name
 s name="m_client"
 i $g(%zcs("client"))="z" s name="m_php"
 i $g(%zcs("client"))="j" s name="m_jsp"
 i $g(%zcs("client"))="a" s name="m_aspx"
 i $g(%zcs("client"))="p" s name="m_python"
 i $g(%zcs("client"))="r" s name="m_ruby"
 i $g(%zcs("client"))="h" s name="m_apache"
 i $g(%zcs("client"))="c" s name="m_cgi"
 i $g(%zcs("client"))="w" s name="m_websphere_mq"
 q name
 ;
set ; global set
 i argc<3 q
 s argz=argc-1
 s fun=0 d ref
 x "s "_ref_"="_"req"_argc
 d res
 q
 ;
get ; global get
 i argc<2 q
 s argz=argc
 s fun=0 d ref
 x "s res=$g("_ref_")"
 d res
 q
 ;
kill ; global kill
 i argc<1 q
 s argz=argc
 s fun=0 d ref
 x "k "_ref
 d res
 q
 ;
data ; global get
 i argc<2 q
 s argz=argc
 s fun=0 d ref
 x "s res=$d("_ref_")"
 d res
 q
 ;
order ; global order
 i argc<3 q
 s argz=argc
 s fun=0 d ref
 x "s res=$o("_ref_")"
 d res
 q
 ;
previous ; global reverse order
 i argc<3 q
 s argz=argc
 s fun=0 d ref
 x "s res=$o("_ref_",-1)"
 d res
 q
 ;
increment ; Global increment
 i argc<3 q
 s argz=argc-1
 s fun=0 d ref
 x "s res=$i("_ref_","_"req"_argc_")"
 d res
 Q
 ;
mergedb ; global merge from m_client
 i argc<3 q
 s a="" f argz=1:1 q:'$d(req(argz))  i $g(req(argz,0))=1 s a=req(argz) q
 i a="" q
 s argz=argz-1
 s fun=0 d ref
 i ref["()" s ref=$p(ref,"()",1)
 i $g(@req(argz+2))["ks" x "k "_ref
 x "m "_ref_"="_a
 d res
 q
 ;
mergephp ; global merge to m_client
 i argc<3 q
 s a="" f argz=1:1 q:'$d(req(argz))  i $g(req(argz,0))=1 s a=req(argz) q
 i a="" q
 s argz=argz-1
 s fun=0 d ref
 i ref["()" s ref=$p(ref,"()",1)
 x "m "_a_"="_ref
 s argz=argz+1
 s abyref=1
 d res
 q
 ;
html ; html
 i '$d(%zcs("ifc")) s res(1)=$c(1,2,1,10)_"0sc"_$c(10) w res(1)
 i argc<2 q
 s argz=argc
 s fun=0 d ref
 x "n ("_refn_") d "_ref
 q
 ;
htmlm ; html (cos) method
 i '$d(%zcs("ifc")) s res(1)=$c(1,2,1,10)_"0sc"_$c(10) w res(1)
 i argc<1 q
 s argz=argc
 s fun=0 d oref
 s ref=$tr($p(ref,",",1,2),".","")_","_$p(ref,",",3,999)
 i argc=1 x "n ("_refn_") s req(-1)=$ClassMethod()"
 i argc>1 x "n ("_refn_") s req(-1)=$ClassMethod("_ref_")"
 s res=$g(req(-1))
 q
 ;
web ; web request
 n req,x,y,i,websocket,status
 new $ztrap set $ztrap="zgoto "_$zlevel_":webe^%zmgsis"
 s res(1)=$c(1,2,1,10)_"0sc"_$c(10) w res(1)
 i argc'=4 g webe
 s argz=argc
 s fun=1 d ref
 s x="" f  s x=$o(req4(x)) q:x=""  s y="" f i=1:1 s y=$o(req4(x,y)) q:y=""  i y'=i m req4(x,i)=req4(x,y) k req4(x,y)
 s websocket=0 i $d(req3("HTTP_SEC_WEBSOCKET_KEY")) s websocket=1
 i 'websocket  x "n ("_refn_") d "_ref g webx
 ; websockets
 k ^%zmgsi("ws",$j)
 x "n ("_refn_") d "_ref
 i $g(^%zmgsi("ws","status"))'=-2 s status=$$wsexit()
 k ^%zmgsi("ws",$j)
 c 0
 s conc=1,dev=$i d halt
webx ; exit
 q
webe ; error
 i $d(req3("HTTP_SEC_WEBSOCKET_KEY")) d event^%zmgsis("websocket error: "_$$error())
 s req=""
 s req=req_"HTTP/1.1 503 SERVICE UNAVAILABLE"_$c(13,10)
 s req=req_"Content-Type: text/plain"_$c(13,10)
 s req=req_"Connection: close"_$c(13,10)
 s req=req_$c(13,10)
 s req=req_"error calling web function "_$g(ref)_$g(refn)_$c(13,10)
 s req=req_$$error()
 s req=req_"web functions contain two arguments"_$c(13,10)
 w req
 q
 ;
webex(cgi,data)
 n req,x,y
 s req=""
 s req=req_"HTTP/1.1 200 OK"_$c(13,10)
 s req=req_"Content-Type: text/plain"_$c(13,10)
 s req=req_"Connection: close"_$c(13,10)
 s req=req_$c(13,10)
 w req
 w "Server="_$zv_$c(13,10)
 w "Mgsi_Version="_$$v()_$c(13,10)
 w $c(13,10)
 s x="" f  s x=$o(cgi(x)) q:x=""  w x_"="_$g(cgi(x))_$c(13,10)
 w $c(13,10)
 s x="" f  s x=$o(data(x)) q:x=""  s y="" f  s y=$o(data(x,y)) q:y=""  w x_":"_y_"="_$g(data(x,y))_$c(13,10)
 q
 ;
ws(cgi,data)
 n status,data,timeout
 new $ztrap set $ztrap="zgoto "_$zlevel_":wse^%zmgsis"
 s status=$$wsstart^%zmgsis()
 s timeout=10
 s data=""
 f i=1:1:10 d  i data="exit" q
 . n i
 . s status=$$wsread^%zmgsis(.data,timeout)
 . i status=-2 s data="exit" q
 . i status=-1 s data="timeout event ("_timeout_"): $h="_$h_"; $zv="_$zv
 . i data="exit" q
 . s status=$$wswrite^%zmgsis(data)
 . q
 q
wse ; error
 d event^%zmgsis("websocket test error: "_$$error())
 q
 ;
wsstart() ; accept request for websocket server
 n status,res
 s status=1
 s res=""
 s res=res_"HTTP/1.1 200 OK"_$char(13,10)
 s res=res_"Content-Type: text/html"_$char(13,10)
 s res=res_"Connection: close"_$char(13,10)
 s res=res_$char(13,10)
 w res
 d flush
 d end
 q status
 ;
wsreject ; reject request for websocket server
 n status,res
 s status=1
 s res=""
 s res=res_"HTTP/1.1 403 FORBIDDEN"_$char(13,10)
 s res=res_"Content-Type: text/html"_$char(13,10)
 s res=res_"Connection: close"_$char(13,10)
 s res=res_$char(13,10)
 w res
 d flush
 q status
 ;
 ;s x=$$wsehead(i,7),z=$$wsdhead(x,.y)
wsread(data,timeout) ; websocket - read from client
 n status,head,type
 new $ztrap set $ztrap="zgoto "_$zlevel_":wsreade^%zmgsis"
 s status=1
 r head#5:timeout e  s status=0
 i 'status s status=-1 g wsreadx
 i $l(head)'=5 s status=-2 g wsreadx
 s size=$$wsdhead(head,.type)
 r data#size:timeout e  s status=0
wsreadx ; exit
 i status=-2 s ^%zmgsi("ws",$j,"status")=status
 q status
wsreade ; error
 s ^%zmgsi("ws",$j,"status")=-2
 q -2
 ;
wswrite(data) ; websocket - write to client
 n status,head,type,size
 new $ztrap set $ztrap="zgoto "_$zlevel_":wswritee^%zmgsis"
 s status=1
 s type=0
 s size=$l(data)
 s head=$$wsehead(size,type)
 w (head_data)
 d flush
 q status
wswritee ; error
 q -1
 ;
wsexit() ; websocket - end server session
 n status,size,type
 s status=1
 s size=0
 s type=3
 s head=$$wsehead(size,type)
 w head
 d flush
 q status
 ;
http ; http
 new $ztrap set $ztrap="zgoto "_$zlevel_":httpe^%zmgsis"
 i '$d(%zcs("ifc")) s res(1)=$c(1,2,1,10)_"0sc"_$c(10) w res(1)
 i argc<2 q
 s x=$$http1(.req2,@req(3))
httpx ; exit
 k ^%zmg("mpc",$j,"content")
 q
httpe ; error
 w "<html><head><title>m_client: error</title></head><h2>extc is not installed on this computer<h2></html>"
 q
 ;
http1(%cgievar,content) ; http
 n (%cgievar,content)
 s test=0
 i test d  q 0
 . w "<br><b>cgi</b>"
 . s x="" f  s x=$o(%cgievar(x)) q:x=""  w "<br>"_x_"="_$g(%cgievar(x))
 . w "<br><b>content</b>"
 . s x="" f  s x=$o(^%zmg("mpc",$j,"content",x)) q:x=""  w "<br>"_x_"="_$g(^%zmg("mpc",$j,"content",x))
 . q
 i $g(%cgievar("key_extcserver"))="true" d  quit 1
 . ; break out to extc server here
 . s namespace=$g(%cgievar("key_namespace"))
 . s:namespace'="" namespace=$g(^%extc("system","phpsettings","namespace",namespace))
 . s:namespace="" namespace=$g(^%extc("system","phpsettings","defaultnamespace"))
 . s:namespace="" namespace="%cachelib"
 . d phpserver^%exmlserver
 quit 0
 ; 
proc ; m extrinsic function
 i argc<2 q
 s argz=argc
 s fun=1 d ref
 i argc=2 x "n ("_refn_") s req(-1)=$$"_ref_"()"
 i argc>2 x "n ("_refn_") s req(-1)=$$"_ref
 s res=$g(req(-1))
 d res
 q
 ;
meth ; m (cos) method
 ;
 ; synopsis:
 ; s err=$ClassMethod(classname,methodname,param1,...,paramn)
 ;
 ; s classname="extc.domapi"
 ; s methodname="opendom"
 ; s err=$ClassMethod(classname,methodname)
 ;
 ; ; this is equivalent to ...
 ; s err=##class(extc.domapi).opendom()
 ; 
 ; s methodname="getdocumentnode"
 ; s documentname="extcdom2"
 ; s err=$ClassMethod(classname,methodname,documentname)
 ; ; this is equivalent to ...
 ; s err=##class(extc.domapi).getdocumentnode("extcdom2")
 ;
 i argc<1 q
 s argz=argc
 s fun=1 d oref
 s ref=$tr($p(ref,",",1,2),".","")_","_$p(ref,",",3,999)
 i argc=1 x "n ("_refn_") s req(-1)=$ClassMethod()"
 i argc>1 x "n ("_refn_") s req(-1)=$ClassMethod("_ref_")"
 s res=$g(req(-1))
 d res
 q
 ;
sort(a) ; sort an array
 q 1
 ;
ref ; global reference
 n com,i,strt,a1
 s array=0,refn="res,req,extra,global,oversize"
 i argc<2 q
 s a1=$g(@req(2))
 s strt=2 i a1?1"^"."^" s strt=strt+1
 s ref=@req(strt) i argc=strt q
 i strt'<argz q
 s ref=ref_"("
 s com="" f i=strt+1:1:argz s refn=refn_","_req(i),ref=ref_com_$s(fun:".",1:"")_req(i),com=","
 s ref=ref_")"
 q
 ;
oref ; object reference
 n com,i,strt,a1
 s array=0,refn="req,extra,global,oversize"
 i argc<2 q
 s a1=$g(@req(2))
 s strt=2 i a1?1"^"."^" s strt=strt+1
 i '$d(req(strt)) q
 s ref=""
 s com="" f i=strt:1:argz s refn=refn_","_req(i),ref=ref_com_$s(fun:".",1:"")_req(i),com=","
 q
 ;
res ; return result
 n i,a,sn,argc
 s maxlen=$$getslen()
 d vars
 s anybyref=0 f argc=1:1:argz q:'$d(req(argc))  i $g(req(argc,1)) s anybyref=1 q
 i 'anybyref d  q
 . d send($g(res))
 . i oversize f sn=1:1 q:'$d(^mgsi($j,0,extra,sn))  d send($g(^(sn)))
 . q
 s res(1)="00000cc"_$c(10)
 s size=$l($g(res)),byref=0
 i oversize f sn=1:1 q:'$d(^mgsi($j,0,extra,sn))  s size=size+$l(^(sn))
 s x=$$ehead(.head,size,byref,ddata)
 d send(head)
 d send($g(res))
 i oversize f sn=1:1 q:'$d(^mgsi($j,0,extra,sn))  d send($g(^(sn)))
 f argc=1:1:argz q:'$d(req(argc))  d
 . s byref=$g(req(argc,1))
 . s array=$g(req(argc,0))
 . i 'byref s size=0,x=$$ehead(.head,size,byref,ddata) d send(head) q
 . i 'array d  q
 . . s size=$l($g(@req(argc)))
 . . f sn=1:1 q:'$d(@(req(argc)_"(extra,sn)"))  s size=size+$l($g(@(req(argc)_"(extra,sn)")))
 . . s x=$$ehead(.head,size,byref,ddata)
 . . d send(head)
 . . d send($g(@req(argc)))
 . . f sn=1:1 q:'$d(@(req(argc)_"(extra,sn)"))  d send($g(@(req(argc)_"(extra,sn)")))
 . . q
 . s x=$$ehead(.head,0,0,darec)
 . d send(head)
 . d resa
 . s x=$$ehead(.head,0,0,deod)
 . d send(head)
 . q
 q
 ;
res1 ; link to array parser
 d send($g(req(argc,0))_$g(req(argc,1)))
 i '$g(req(argc,1)) q
 i $g(req(argc,0))=0 d send($g(@req(argc))) q
 s a=req(argc),fkey="" i global s a="^mgsi",fkey="$j,argc-2"
 d resa
 q
 ;
resa ; return array
 n ref,kn,ok,def,data,txt
 new $ztrap set $ztrap="zgoto "_$zlevel_":resae^%zmgsis"
 s byref=0
 s a=req(argc),fkey="" i global s a="^mgsi",fkey="$j,argc-2"
 i a="" q
 i global d
 . i ($d(@(a_"("_fkey_")"))#10) d
 . . s txt=" ",x=$$ehead(.head,$l(txt),byref,dakey) d send(head),send(txt)
 . . s size=0
 . . s size=size+$l($g(@req(argc)))
 . . f sn=1:1 q:'$d(@(a_"("_fkey_","_"extra,sn)"))  s size=size+$l($g(^(sn)))
 . . s x=$$ehead(.head,size,byref,ddata) d send(head)
 . . d send($g(@req(argc)))
 . . f sn=1:1 q:'$d(@(a_"("_fkey_","_"extra,sn)"))  d send($g(^(sn)))
 . . q
 . s fkey=fkey_","
 . q
 i 'global d
 . i ($d(@a)#10),$l($g(@a)) d
 . . s txt=" ",x=$$ehead(.head,$l(txt),byref,dakey) d send(head),send(txt)
 . . s size=0
 . . s size=size+$l($g(@a))
 . . f sn=1:1 q:'$d(@(a_"(extra,sn)"))  s size=size+$l($g(@(a_"(extra,sn)")))
 . . s x=$$ehead(.head,size,byref,ddata) d send(head)
 . . d send($g(@a))
 . . f sn=1:1 q:'$d(@(a_"(extra,sn)"))  d send($g(@(a_"(extra,sn)")))
 . . q
 . q
 s ok=0,kn=1,x(kn)="",ref="x("_kn_")"
 f  s x(kn)=$o(@(a_"("_fkey_ref_")")) d resa1 i ok q
 q
resae ; error
 new $ztrap set $ztrap="zgoto "_$zlevel_":"
 q
 ;
resa1 ; array node
 i x(kn)=extra q
 i x(kn)="",kn=1 s ok=1 q
 i x(kn)="" s kn=kn-1,ref=$p(ref,",",1,$l(ref,",")-1) q
 s def=$d(@(a_"("_fkey_ref_")")) i (def\10) d resa3
 s data=$g(@(a_"("_fkey_ref_")"))
 i (def#10) d resa2
 i (def\10) s kn=kn+1,x(kn)="",ref=ref_","_"x("_kn_")"
 q
 ;
resa2 ; array node data
 n i,spc
 f i=1:1:kn s x=$$ehead(.head,$l(x(i)),byref,dakey) d send(head),send(x(i))
 i $g(%zcs("client"))="z",(def\10) s spc=" ",x=$$ehead(.head,$l(spc),byref,dakey) d send(head),send(spc)
 s size=$l(data)
 f sn=1:1 q:'$d(@(a_"("_fkey_ref_",extra,sn)"))  s size=size+$l($g(@(a_"("_fkey_ref_",extra,sn)")))
 s x=$$ehead(.head,size,byref,ddata) d send(head)
 d send(data)
 f sn=1:1 q:'$d(@(a_"("_fkey_ref_",extra,sn)"))  d send($g(@(a_"("_fkey_ref_",extra,sn)")))
 q
 ;
resa3 ; array node data with descendants - test for non-extra data
 n y
 s y="" f  s y=$o(@(a_"("_ref_",y)")) q:y=""  i y'=extra q
 i y="" s def=1
 q
 ;
send(data) ; send data
 n n
 ;d event(data_$g(res(1)))
 s n=$o(res(""),-1)
 i n="" s n=1
 i $l($g(res(n)))+$l(data)>maxlen s n=n+1
 s res(n)=$g(res(n))_data
 q
 ;
getsys() ; get system type
 s systype=$s($$isidb()>1:"cache",$$isidb()=1:"ism",$$ism21():"m21",$$ismsm():"msm",$$isdsm():"dsm",$$isydb():"gtm",1:"")
 q systype
 ;
error() ; get last error
 i $$isydb() q $zs
 q $ze
 ;
seterror(v) ; set error
 q
 ;
getuci() ; get namespace/uci
 new $ztrap set $ztrap="zgoto "_$zlevel_":getucie^%zmgsis"
 s %uci=$zg
 q %uci
getucie ; error
 q ""
 ;
getslen() ; get maximum string length
 s slen=$s($$isidb():32000,$$ism21():1023,$$ismsm():250,$$isdsm():250,$$isydb():32000,1:250)
 q slen
 ;
lcase(string) ; convert to lower case
 q $tr(string,"abcdefghijklmnopqrstuvwxyz","abcdefghijklmnopqrstuvwxyz")
 ;
ucase(string) ; convert to upper case
 q $tr(string,"abcdefghijklmnopqrstuvwxyz","abcdefghijklmnopqrstuvwxyz")
 ;
setio(tblname) ; set i/o translation
 new $ztrap set $ztrap="zgoto "_$zlevel_":setioe^%zmgsis"
 q ""
setioe ; error - do nothing
 q ""
 ;
zcvt(buffer,tblname) ; translate buffer
 new $ztrap set $ztrap="zgoto "_$zlevel_":zcvte^%zmgsis"
 q buffer
zcvte ; error - do nothing
 q buffer
 ;
wsmqtest ; test link to websphere mq
 n %zmgmq,response
 w !,"sending test message to mgsi/websphere mq interface..."
 s response=$$wsmq("127.0.0.1",7040,"test",.%zmgmq)
 w !,"response: ",response," ",$g(%zmgmq("recv"))
 q
 ;
wsmqtstx(ip,port) ; test link to websphere mq: ip address and port specified
 n %zmgmq,response
 w !,"sending test message to mgsi/websphere mq interface..."
 s response=$$wsmq(ip,port,"test",.%zmgmq)
 w !,"response: ",response," ",$g(%zmgmq("recv"))
 q
 ;
htest ; return html
 n systype
 s systype=$$getsys()
 w "<i>a line of html from "_systype_"</i>"
 q
 ;
htest1(p1) ; return html
 n x,systype
 s systype=$$getsys()
 ;w "HTTP/1.1 200 OK",$c(13,10)
 ;w "Content-Type: text/html",$c(13,10)
 ;w "Connection: close",$c(13,10)
 ;w $c(13,10)
 w "<i>html content returned from "_systype_"</i>"
 w "<br><i>the input parameter passed was: <b>"_$g(p1)_"</b></i>"
 s x="" f  s x=$o(p1(x)) q:x=""  w "<br><i>array element passed: <b>"_x_" = "_$g(p1(x))_"</b></i>"
 q
 ;
ptest() ; return result
 n systype
 s systype=$$getsys()
 q "result from "_systype_" process: "_$j_"; namespace: "_$$getuci()
 q
 ;
ptest1(p1) ; return result
 n systype
 s systype=$$getsys()
 q "result from "_systype_" process: "_$j_"; namespace: "_$$getuci()_"; the input parameter passed was: "_p1
 ;
ptest2(p1,p2) ; manipulate an array
 n n,x,systype
 s systype=$$getsys()
 s n=0,x="" f  s x=$o(p1(x)) q:x=""  s n=n+1
 s p1("key from m - 1")="value 1"
 s p1("key from m - 2")="value 2"
 ; s p1("key from m - 2",extra,1)=" ... more data ..."
 ; s p1("key from m - 2",extra,2)=" ... and more data"
 s p2="stratford"
 ; s ^mgsi($j,0,extra,1)=" ... more output ...",^(2)=" ... and more output",oversize=1
 q "result from "_systype_" process: "_$j_"; namespace: "_$$getuci()_"; "_n_" elements were received in an array, 2 were added by this procedure"
 ;
setunpw(username,password)
 s ^%zmgsi(0,"username")=username
 s ^%zmgsi(0,"password")=password
 q 1
 ;
getunpw(username,password)
 s username=$g(^%zmgsi(0,"username"))
 s password=$g(^%zmgsi(0,"password"))
 q 1
 ;
checkunpw(username,password)
 n x,username1,password1
 s x=$$getunpw(.username1,.password1)
 i username1="",password1="" q 1
 i username1'=username d event("access violation: bad username") q 0
 i password1'=password d event("access violation: bad password") q 0
 q 1
 ;
sst(%0) ; save symbol table
 new $ztrap set $ztrap="zgoto "_$zlevel_":sste^%zmgsis"
 n %
 k @%0
 s %0=$e(%0,1,$l(%0)-1)
 s %="%" f  s %=$o(@%) q:%=""  i %'="%0" m @(%0_",%)")=@%
 q 1
sste ; error
 q 0
 ;
dbx(buf) ; new wire protocol for access to M
 n %oref,abyref,argc,array,cmnd,conc,dakey,darec,ddata,deod,extra,global,i,maxlen,mqinfo,nato,offset,ok,oref,oversize,pcmnd,port,pport,rdxbuf,rdxptr,rdxrlen,rdxsize,req,res,sl,slen,sn,sort,type,uci,var,version,x
 s uci=$p(buf,"~",2)
 i uci'="" d uci(uci)
 s res=$zv
 s res=$$esize256($l(res))_"0"_res
 w res d flush
dbx1 ; test
 r head#5
 s len=$$dsize256(head)
 s len=len-5
 s cmnd=$e(head,5)
 r data#len
 s obufsize=$$dsize256($e(data,1,4))
 s idx=$$dsize256($e(data,6,9))
 k %r s offset=11 f %r=1:1:64 s %r(%r,0)=$$dsize256($e(data,offset,offset+3)) d  i '$d(%r(%r)) s %r=%r-1 q
 . s %r(%r,1)=$a(data,offset+4)\20,%r(%r,2)=$a(data,offset+4)#20 i %r(%r,1)=9 k %r(%r) q
 . s %r(%r)=$e(data,offset+5,offset+5+(%r(%r,0)-1))
 . s offset=offset+5+%r(%r,0)
 . q
 s rc=$$dbxcmnd(.%r,.%oref,$a(cmnd),.res)
 i rc=0 s sort=1 ; data
 i rc=-1 s sort=11 ; error
 s type=1 ; string
 s res=$$esize256($l(res))_$c((sort*20)+type)_res
 w res d flush
 g dbx1
 ;
dbxcmnd(%r,%oref,cmnd,res) ; Execute command
 new $ztrap set $ztrap="zgoto "_$zlevel_":dbxcmnde^%zmgsis"
 n buf,data,head,idx,len,obufsize,offset,rc,uci
 s res=""
 i cmnd=2 s res=0 q 0
 i cmnd=3 s res=$$getuci() q 0
 i cmnd=4 d uci(%r(1)) s res=$$getuci() q 0
 i cmnd=11 s @(%r(1)_$$dbxref(.%r,2,%r-1,0))=%r(%r),res=0 q 0
 i cmnd=12 s res=$g(@(%r(1)_$$dbxref(.%r,2,%r,0))) q 0
 i cmnd=13 s res=$o(@(%r(1)_$$dbxref(.%r,2,%r,0))) q 0
 i cmnd=14 s res=$o(@(%r(1)_$$dbxref(.%r,2,%r,0)),-1) q 0
 i cmnd=15 k @(%r(1)_$$dbxref(.%r,2,%r,0)) s res=0 q 0
 i cmnd=16 s res=$d(@(%r(1)_$$dbxref(.%r,2,%r,0))) q 0
 i cmnd=17 s res=$i(@(%r(1)_$$dbxref(.%r,2,%r-1,0)),%r(%r)) q 0
 i cmnd=31 s res=$$dbxfun(.%r,"$$"_%r(1)_"("_$$dbxref(.%r,2,%r,1)_")") q 0
 s res="<SYNTAX>"
 q -1
dbxcmnde ; Error
 s ze=$$error()
 q -1
 ;
dbxref(%r,strt,end,ctx) ; Generate reference
 n i,ref,com
 s ref="",com="" f i=strt:1:end s ref=ref_com_"%r("_i_")",com=","
 i ctx=0,ref'="" s ref="("_ref_")"
 q ref
 ;
dbxfun(%r,fun) ; Execute function
 n %oref,%uci,a,buf,cmnd,data,head,idx,len,obufsize,offset,oref,rc,res,sort,type,uci
 s @("res="_fun)
 q res
 ;
dbxcmeth(%r,cmeth) ; Execute function
 n %oref,%uci,a,buf,cmnd,data,head,idx,len,obufsize,offset,oref,rc,res,sort,type,uci
 s @("res=$ClassMethod("_cmeth_")")
 q res
 ;
dbxmeth(%r,meth) ; Execute function
 n %oref,%uci,a,buf,cmnd,data,head,idx,len,obufsize,offset,oref,rc,res,sort,type,uci
 s @("res=$Method("_meth_")")
 q res
 ;
 ; s x=$$sqleisc^%zmgsis(0,"SELECT * FROM SQLUser.customer","")
sqleisc(id,sql,params) ; Execute InterSystems SQL query
 new $ztrap set $ztrap="zgoto "_$zlevel_":sqleisce^%zmgsis"
 n %objlasterror,result,error,data,status,tsql,cn,col,rset,rn,n,sort,type
 k ^mgsqls($j,id)
 s result="0",error="",data="",cn=0
 s error="InterSystems SQL not available with YottaDB" g sqleisce1
 s sort=1,type=1,result=$$esize256($l(cn))_$c((sort*20)+type)_cn_data
 q $$esize256($l(result))_result
sqleisce ; M error
 s error=$$error()
sqleisce1 ; SQL error
 s sort=11,type=1,result=$$esize256($l(error))_$c((sort*20)+type)_error
 q $$esize256($l(result))_result
 ;
 ; s x=$$sqlemg^%zmgsis(0,"select * from patient","")
sqlemg(id,sql,params) ; Execute MGSQL SQL query
 new $ztrap set $ztrap="zgoto "_$zlevel_":sqlemge^%zmgsis"
 n %zi,%zo,result,error,data,cn,n,col,ok,sort,type,v
 s result="0",error="",data="",cn=0
 s %zi("stmt")=id
 s v=$$sqlmgv() i v="" s error="MGSQL not installed" g sqlemge1
 s ok=$$exec^%mgsql("",sql,.%zi,.%zo)
 i $d(%zo("error")) s error=$g(%zo("error")) g sqlemge1
 s sort=1,type=1
 f n=1:1 q:'$d(%zo(0,n))  s col=$g(%zo(0,n)) d
 . i col["." s col=$p(col,".",2)
 . s col=$tr(col,"-_.;","")
 . i col="" s col="column_"_n
 . s ^mgsqls($j,%zi("stmt"),0,0,n)=col
 . s data=data_$$esize256($l(col))_$c((sort*20)+type)_col
 . q
 s cn=n-1
 s sort=1,type=1,result=$$esize256($l(cn))_$c((sort*20)+type)_cn_data
 q $$esize256($l(result))_result
sqlemge ; M error
 s error=$$error()
sqlemge1 ; SQL error
 s sort=11,type=1,result=$$esize256($l(error))_$c((sort*20)+type)_error
 q $$esize256($l(result))_result
 ;
sqlmgv() ; get MGSQL version
 new $ztrap set $ztrap="zgoto "_$zlevel_":sqlmgve^%zmgsis"
 s v=$$v^%mgsql()
 q v
sqlmgve ; M error
 q ""
 ;
sqlrow(id,rn,params) ; Get a row
 n result,data,n,sort,type
 s result=""
 s sort=1,type=1
 i params["-1" s:rn=0 rn="" s rn=$o(^mgsqls($j,id,0,rn),-1) i rn=0 s rn=""
 i params["+1" s:rn="" rn=0 s rn=$o(^mgsqls($j,id,0,rn))
 i rn="" q result
 i '$d(^mgsqls($j,id,0,rn)) q result
 s result=result_$$esize256($l(rn))_$c((sort*20)+type)_rn
 f n=1:1 q:'$d(^mgsqls($j,id,0,rn,n))  s data=$g(^mgsqls($j,id,0,rn,n)),result=result_$$esize256($l(data))_$c((sort*20)+type)_data
 q $$esize256($l(result))_result
 ;
sqldel(id,params) ; Delete SQL result set
 k ^mgsqls($j,id)
 q ""
 ;
