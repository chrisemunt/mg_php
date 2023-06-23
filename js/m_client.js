// (CM) Client for the m_* technologies implemented over XMLHTTP.  7 January 2005.
//      Copyright (c) 2002 - 2023 by MGateway Ltd.
//      All Rights Reserved.
//
//      Revised 13 June 2019
//

var m_client_UseGetMethod                = false;

var m_client_max_cc_requests             = 8;

var m_client_status_free                 = 0;
var m_client_status_inuse                = 1;
var m_client_ret_success                 = 1;
var m_client_ret_error                   = -1;

var m_client_XMLHttp                     = [null, null, null, null, null, null, null, null];
var m_client_Mozilla                     = [false, false, false, false, false, false, false, false];

var m_client_Status                      = [0, 0, 0, 0, 0, 0, 0, 0];


function server_proc(DataObject)
{
   var r_handle;
   var result;

   r_handle = m_client_MethodEx_AllocateHandle();

   result = m_client_SendRequest(r_handle, DataObject);

   r_handle = m_client_MethodEx_ReleaseHandle(r_handle);

   return result;

}


function m_client_MethodEx_AllocateHandle()
{
   var r_handle;
   var n;
   var error;

   r_handle = m_client_ret_error;

   for (n = 1; n < m_client_max_cc_requests; n ++) {
      if (m_client_Status[n] == m_client_status_free) {
         m_client_Status[n] = m_client_status_inuse;
         r_handle = n;

	      if (m_client_FindXMLHttp(r_handle) == null) {
            error = 'Unable to locate XMLHttpObject';
            alert(error);
            return m_client_ret_error;
         }

         break;
      }
   }

   return r_handle;
}


function m_client_MethodEx_ReleaseHandle(RequestHandle)
{
   var r_handle;
   var error;

   r_handle = RequestHandle;

   if (r_handle < 0 || r_handle >= m_client_max_cc_requests)
      return -1;

   m_client_Status[r_handle] = m_client_status_free;

   return r_handle;
}


function m_client_FindXMLHttp(r_handle)
{
   if (m_client_XMLHttp[r_handle] != null)
      return m_client_XMLHttp[r_handle];

   /*@cc_on @*/
   /*@if (@_jscript_version >= 5)
      m_client_Mozilla[r_handle] = false;
      try {
         m_client_XMLHttp[r_handle] = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (e) {
         try {
            m_client_XMLHttp[r_handle] = new ActiveXObject("Microsoft.XMLHTTP");
	      } catch (E) {
            m_client_XMLHttp[r_handle] = null;
         }
      }
   @end @*/

   if (m_client_XMLHttp[r_handle] != null)
      return m_client_XMLHttp[r_handle];

   try {
      m_client_XMLHttp[r_handle] = new XMLHttpRequest();
      m_client_Mozilla[r_handle] = true;
   } catch (e) {
      m_client_XMLHttp[r_handle] = null;
   }

   return m_client_XMLHttp[r_handle];
}


function m_client_SendRequest(r_handle, Request)
{
   var n;
   var error;
   var url_string;
   var con_string;
   var response;
   var temp;

   error = "";
   response = "";

   try {
      if (m_client_UseGetMethod && !m_client_Mozilla[r_handle]) {

         url_string = Request;

         m_client_XMLHttp[r_handle].open("GET", url_string, false);
         m_client_XMLHttp[r_handle].send();
      }
      else {

         temp = Request.split("?");

         url_string = temp[0];
         con_string = temp[1];
         m_client_XMLHttp[r_handle].open("POST", url_string, false);
         m_client_XMLHttp[r_handle].setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
         m_client_XMLHttp[r_handle].send(con_string);
      }

      response = m_client_XMLHttp[r_handle].responseText;
      n = response.indexOf('\x07');
      if (n >= 0) {
         response = response.substring(n + 1);
      }


   } catch (e) {
      error = 'ERROR: HTTP object request failed - Unable to process m_client request.' + e;
      alert(error);
   }

   return response;
}
